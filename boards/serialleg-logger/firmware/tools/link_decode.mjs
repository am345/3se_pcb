#!/usr/bin/env node
// 把日志板写出的 LOGnnnnn.BIN（DM-MC02 logger_link 帧原始字节流）解码为 CSV。
// 用法：node link_decode.mjs LOG00001.BIN [输出目录]
// 每种帧输出一个 <名>_<类型>.csv（state、motor、event、health、policy、control），并打印帧统计。
// 帧格式与 serialleg2026 Custom/app/telemetry/task_logger_link.h（LOGGER_LINK_VERSION 3）一致；
// 2026-10-27 前该文件名为 task_nx_comm.h，只有 STATE 帧，其三个目标字段已改名为 policy_*。
// 主控改了负载结构后须同步修改下面的字段表；各表长度与主控的 _Static_assert 一致。
// 关节数组均为策略顺序 LF/LB/RF/RB，轮为 L/R。

import { readFileSync, writeFileSync } from 'node:fs';
import { basename, dirname, extname, join } from 'node:path';

const SOF0 = 0xa5;
const SOF1 = 0x5a;
const HEADER = 8;
const CRC = 2;
const MAX_PAYLOAD = 192;
const VERSION = 3;

const vec = (name, n, type = 'f32') => Array.from({ length: n }, (_, i) => [`${name}_${i}`, type]);
const named = (prefix, names, type) => names.map((n) => [`${prefix}_${n}`, type]);
const TASKS = ['control', 'estimate', 'ins', 'decision', 'logger_link'];

// logger_link_state_payload_t
const STATE_FIELDS = [
  ['tick_ms', 'u32'], ['state_seq', 'u32'], ['policy_seq', 'u32'],
  ['policy_age_ms', 'u16'], ['policy_valid', 'u8'], ['rc_switch_r', 'u8'],
  ['output_enabled', 'u8'], ['_reserved', 'skip3'],
  ...vec('base_ang_vel', 3), ...vec('projected_gravity', 3),
  ...vec('joint_pos', 4), ...vec('joint_vel', 4),
  ...vec('wheel_pos', 2), ...vec('wheel_vel', 2),
  ...vec('target_joint_pos', 4), ...vec('hip_torque', 4),
  ...vec('wheel_torque', 2), ...vec('wheel_motor_torque', 2),
  ...vec('command', 3), ['jump_target_height', 'f32'],
];

// logger_link_motor_payload_t；quat 为 QEKF 的 w, x, y, z（IMU 坐标系）
const MOTOR_FIELDS = [
  ['tick_ms', 'u32'], ...named('quat', ['w', 'x', 'y', 'z'], 'f32'),
  ['qekf_flags', 'u8'], ['hip_dead_mask', 'u8'],
  ...vec('hip_state', 4, 'u8'), ...vec('hip_t_mos', 4, 'u8'), ...vec('hip_t_coil', 4, 'u8'),
  ...vec('wheel_temp', 2, 'i8'), ...vec('accel', 3), ['imu_temp', 'f32'],
  ...vec('hip_torque_meas', 4), ...vec('wheel_torque_meas', 2),
];

// logger_link_event_payload_t
const EVENT_FIELDS = [
  ['tick_ms', 'u32'], ['id', 'u16'], ['index', 'u8'], ['_reserved', 'skip1'], ['value', 'i32'],
];
const EVENT_NAMES = {
  1: 'enable', 2: 'rc_link', 3: 'policy_stage', 4: 'policy_error', 5: 'hip_cmd_error',
  6: 'motor_online', 7: 'hip_state', 8: 'can_bus_off', 9: 'can_recover',
  10: 'deadline_miss', 11: 'queue_drop',
};

// logger_link_health_payload_t
const canFields = (k) => [
  [`can${k}_tx_fail`, 'u32'], [`can${k}_bus_off`, 'u32'], [`can${k}_recover`, 'u32'],
  [`can${k}_rx_fifo0_peak`, 'u16'], [`can${k}_tx_error_cnt`, 'u8'], [`can${k}_rx_error_cnt`, 'u8'],
];
const HEALTH_FIELDS = [
  ['tick_ms', 'u32'],
  ...named('task_max_ms', TASKS, 'f32'), ...named('task_out_of_range', TASKS, 'u32'),
  ...canFields(1), ...canFields(2), ...canFields(3),
  ['policy_inferences', 'u32'], ['policy_deadline_misses', 'u32'], ['policy_max_cycles', 'u32'],
  ['policy_last_period_ms', 'u16'], ['policy_max_period_ms', 'u16'], ['policy_resets', 'u16'],
  ['policy_stage', 'u8'], ['policy_error', 'i8'],
  ['link_tx_ok', 'u32'], ['link_tx_busy', 'u32'], ['link_tx_fail', 'u32'], ['link_queue_drop', 'u32'],
  ['link_queue_peak_bytes', 'u16'], ['rc_frame_counter', 'u16'], ['rc_online', 'u8'], ['_reserved', 'skip3'],
  ['supercap_volt', 'f32'],
];

// logger_link_policy_payload_t；input 为 policy_input_t（joint_position/velocity 含两轮，共 6 项）
const POLICY_FIELDS = [
  ['tick_ms', 'u32'], ['policy_seq', 'u32'], ['input_tick_ms', 'u32'], ['infer_cycles', 'u32'],
  ['status', 'i8'], ['stage', 'u8'], ['_reserved', 'skip2'],
  ...vec('in_ang_vel', 3), ...vec('in_gravity', 3), ...vec('in_joint_pos', 6), ...vec('in_joint_vel', 6),
  ...vec('in_command', 3), ['in_jump_request', 'f32'],
  ...vec('raw_action', 6), ...vec('leg_target', 4), ...vec('wheel_target', 2),
];

// logger_link_control_payload_t
const CONTROL_FIELDS = [
  ['snapshot_us', 'u32'], ['tick_ms', 'u32'], ['policy_seq', 'u32'],
  ['flags', 'u8'], ['valid_mask', 'u8'], ['hip_command_error', 'i8'], ['_reserved', 'skip1'],
  ...vec('joint_pos', 4), ...vec('joint_vel', 4), ...vec('joint_target', 4),
  ...vec('hip_torque_cmd', 4), ...vec('hip_torque_meas', 4),
  ...vec('wheel_vel', 2), ...vec('wheel_target', 2), ...vec('wheel_torque_cmd', 2), ...vec('wheel_torque_meas', 2),
  ...vec('gyro', 3), ...vec('accel', 3),
];

const TYPES = {
  0x01: { name: 'state', fields: STATE_FIELDS, size: 156 },
  0x10: { name: 'motor', fields: MOTOR_FIELDS, size: 76 },
  0x11: { name: 'event', fields: EVENT_FIELDS, size: 12 },
  0x12: { name: 'health', fields: HEALTH_FIELDS, size: 140 },
  0x14: { name: 'policy', fields: POLICY_FIELDS, size: 156 },
  0x15: { name: 'control', fields: CONTROL_FIELDS, size: 152 },
};

const SIZES = { u8: 1, i8: 1, u16: 2, u32: 4, i32: 4, f32: 4, skip1: 1, skip2: 2, skip3: 3 };
const payloadSize = (fields) => fields.reduce((s, [, t]) => s + SIZES[t], 0);
for (const [type, t] of Object.entries(TYPES)) {
  if (payloadSize(t.fields) !== t.size) throw new Error(`字段表与主控布局不一致：0x${Number(type).toString(16)}`);
}

function crc16(buf, start, end) {
  let crc = 0xffff;
  for (let i = start; i < end; i++) {
    crc ^= buf[i];
    for (let b = 0; b < 8; b++) crc = crc & 1 ? (crc >>> 1) ^ 0x8408 : crc >>> 1;
  }
  return crc;
}

function decodeFields(view, off, fields) {
  const row = [];
  for (const [name, type] of fields) {
    // 保留字段不输出
    if (!name.startsWith('_')) {
      if (type === 'u8') row.push(view.getUint8(off));
      else if (type === 'i8') row.push(view.getInt8(off));
      else if (type === 'u16') row.push(view.getUint16(off, true));
      else if (type === 'u32') row.push(view.getUint32(off, true));
      else if (type === 'i32') row.push(view.getInt32(off, true));
      else row.push(view.getFloat32(off, true));
    }
    off += SIZES[type];
  }
  return row;
}

function header(fields, extra = []) {
  return ['frame_seq', ...fields.filter(([n]) => !n.startsWith('_')).map(([n]) => n), ...extra].join(',');
}

function main() {
  const [input, outDirArg] = process.argv.slice(2);
  if (!input) {
    console.error('用法：node link_decode.mjs LOG00001.BIN [输出目录]');
    process.exit(2);
  }

  const buf = readFileSync(input);
  const view = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
  const outDir = outDirArg ?? dirname(input);
  const stem = basename(input, extname(input));

  const rows = {};
  for (const t of Object.values(TYPES)) {
    rows[t.name] = [header(t.fields, t.name === 'event' ? ['name'] : [])];
  }
  const stats = { bytes: buf.length, frames: 0, other: 0, crc_errors: 0, bad_length: 0,
    bad_version: 0, seq_lost: 0, skipped_bytes: 0 };
  for (const t of Object.values(TYPES)) stats[t.name] = 0;
  let lastSeq = null;
  let i = 0;

  while (i + HEADER + CRC <= buf.length) {
    if (buf[i] !== SOF0 || buf[i + 1] !== SOF1) { i++; stats.skipped_bytes++; continue; }
    const len = view.getUint16(i + 4, true);
    const size = HEADER + len + CRC;
    if (len > MAX_PAYLOAD) { stats.bad_length++; i++; continue; }
    if (i + size > buf.length) break;
    if (crc16(buf, i, i + size - CRC) !== view.getUint16(i + size - CRC, true)) {
      // 从下一个字节重新找帧头，避免因一帧损坏丢掉紧随其后的好帧
      stats.crc_errors++; i++; continue;
    }

    const seq = view.getUint16(i + 6, true);
    if (lastSeq !== null) {
      const step = (seq - lastSeq) & 0xffff;
      if (step > 1) stats.seq_lost += step - 1;
    }
    lastSeq = seq;
    stats.frames++;

    const t = TYPES[buf[i + 2]];
    if (buf[i + 3] !== VERSION) stats.bad_version++;
    else if (t && len === t.size) {
      const row = [seq, ...decodeFields(view, i + HEADER, t.fields)];
      if (t.name === 'event') row.push(EVENT_NAMES[row[2]] ?? 'unknown');
      rows[t.name].push(row.join(','));
      stats[t.name]++;
    } else stats.other++;
    i += size;
  }
  stats.skipped_bytes += buf.length - i;

  for (const t of Object.values(TYPES)) {
    if (t.name === 'state' || stats[t.name] > 0) {
      writeFileSync(join(outDir, `${stem}_${t.name}.csv`), rows[t.name].join('\n') + '\n');
    }
  }
  console.log(JSON.stringify(stats, null, 2));
}

main();
