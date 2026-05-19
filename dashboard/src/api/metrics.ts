export interface ServerStats {
  total_commands: number;
  total_hits: number;
  total_misses: number;
  active_connections: number;
  key_count: number;
  memory_bytes: number;
  avg_latency_us: number;
  hit_rate: number;
  commands_per_sec: number;
}

const BASE = import.meta.env.VITE_METRICS_URL ?? '';

export async function fetchStats(): Promise<ServerStats> {
  const res = await fetch(`${BASE}/api/stats`);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json() as Promise<ServerStats>;
}
