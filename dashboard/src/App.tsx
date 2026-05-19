import { useEffect, useState, useCallback } from 'react'
import { fetchStats, type ServerStats } from './api/metrics'
import {
  LineChart, Line, XAxis, YAxis, Tooltip,
  ResponsiveContainer, CartesianGrid, Legend,
} from 'recharts'

const POLL_MS = 1500
const HISTORY_LEN = 60

interface DataPoint extends ServerStats {
  ts: string
}

function StatCard({ label, value, sub }: { label: string; value: string; sub?: string }) {
  return (
    <div style={{
      background: '#1e2030',
      borderRadius: 8,
      padding: '20px 24px',
      minWidth: 160,
      flex: '1 1 160px',
    }}>
      <p style={{ color: '#94a3b8', fontSize: 12, marginBottom: 4 }}>{label}</p>
      <p style={{ fontSize: 28, fontWeight: 700, color: '#f1f5f9' }}>{value}</p>
      {sub && <p style={{ color: '#64748b', fontSize: 11, marginTop: 2 }}>{sub}</p>}
    </div>
  )
}

function fmt(n: number, decimals = 0) {
  if (n >= 1e9) return (n / 1e9).toFixed(1) + 'B'
  if (n >= 1e6) return (n / 1e6).toFixed(1) + 'M'
  if (n >= 1e3) return (n / 1e3).toFixed(1) + 'K'
  return n.toFixed(decimals)
}

export default function App() {
  const [stats, setStats]       = useState<ServerStats | null>(null)
  const [history, setHistory]   = useState<DataPoint[]>([])
  const [error, setError]       = useState<string | null>(null)
  const [lastUpdate, setLastUpdate] = useState<string>('')

  const poll = useCallback(async () => {
    try {
      const s = await fetchStats()
      setStats(s)
      setError(null)
      const ts = new Date().toLocaleTimeString()
      setLastUpdate(ts)
      setHistory(prev => [...prev.slice(-(HISTORY_LEN - 1)), { ...s, ts }])
    } catch (e) {
      setError((e as Error).message)
    }
  }, [])

  useEffect(() => {
    poll()
    const id = setInterval(poll, POLL_MS)
    return () => clearInterval(id)
  }, [poll])

  const hitPct = stats ? (stats.hit_rate * 100).toFixed(1) : '—'
  const memMb  = stats ? (stats.memory_bytes / 1024 / 1024).toFixed(1) : '—'

  return (
    <div style={{ maxWidth: 1200, margin: '0 auto', padding: '32px 24px' }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', marginBottom: 32 }}>
        <div style={{
          width: 10, height: 10, borderRadius: '50%',
          background: error ? '#ef4444' : '#22c55e',
          marginRight: 10,
        }} />
        <h1 style={{ fontSize: 20, fontWeight: 700, color: '#f1f5f9' }}>
          cacheserver <span style={{ color: '#64748b', fontWeight: 400, fontSize: 14 }}>admin</span>
        </h1>
        <span style={{ marginLeft: 'auto', color: '#475569', fontSize: 12 }}>
          {error ? `error: ${error}` : `updated ${lastUpdate}`}
        </span>
      </div>

      {/* Stat cards */}
      <div style={{ display: 'flex', gap: 12, flexWrap: 'wrap', marginBottom: 32 }}>
        <StatCard label="Active connections" value={stats ? String(stats.active_connections) : '—'} />
        <StatCard label="Keys stored"        value={stats ? fmt(stats.key_count) : '—'} />
        <StatCard label="Commands/sec"       value={stats ? fmt(stats.commands_per_sec) : '—'} />
        <StatCard label="Hit rate"           value={`${hitPct}%`}
                  sub={`${fmt(stats?.total_hits ?? 0)} hits / ${fmt(stats?.total_misses ?? 0)} misses`} />
        <StatCard label="Avg latency"        value={stats ? `${stats.avg_latency_us.toFixed(1)} μs` : '—'} />
        <StatCard label="Memory"             value={stats ? `${memMb} MB` : '—'} sub="estimated RSS" />
      </div>

      {/* Charts */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 16 }}>
        <ChartCard title="Commands / sec" data={history} lines={[
          { key: 'commands_per_sec', color: '#6366f1', label: 'cmds/s' },
        ]} />
        <ChartCard title="Cache hit rate" data={history} lines={[
          { key: 'hit_rate', color: '#22c55e', label: 'hit rate', fmt: (v) => `${(v * 100).toFixed(1)}%` },
        ]} domain={[0, 1]} />
        <ChartCard title="Active connections" data={history} lines={[
          { key: 'active_connections', color: '#f59e0b', label: 'connections' },
        ]} />
        <ChartCard title="Avg latency (μs)" data={history} lines={[
          { key: 'avg_latency_us', color: '#f43f5e', label: 'latency μs' },
        ]} />
      </div>

      <p style={{ color: '#334155', fontSize: 11, marginTop: 24, textAlign: 'right' }}>
        cacheserver v0.1.0 · polling every {POLL_MS}ms
      </p>
    </div>
  )
}

interface LineSpec {
  key: keyof DataPoint
  color: string
  label: string
  fmt?: (v: number) => string
}

function ChartCard({
  title, data, lines, domain,
}: {
  title: string
  data: DataPoint[]
  lines: LineSpec[]
  domain?: [number, number]
}) {
  return (
    <div style={{ background: '#1e2030', borderRadius: 8, padding: '20px 16px' }}>
      <p style={{ color: '#94a3b8', fontSize: 13, marginBottom: 16 }}>{title}</p>
      <ResponsiveContainer width="100%" height={180}>
        <LineChart data={data}>
          <CartesianGrid strokeDasharray="3 3" stroke="#2d3148" />
          <XAxis dataKey="ts" tick={{ fill: '#475569', fontSize: 10 }} interval="preserveStartEnd" />
          <YAxis tick={{ fill: '#475569', fontSize: 10 }} domain={domain ?? ['auto', 'auto']}
                 tickFormatter={lines[0]?.fmt ?? ((v: number) => fmt(v))} />
          <Tooltip
            contentStyle={{ background: '#1e2030', border: '1px solid #334155', borderRadius: 6 }}
            labelStyle={{ color: '#94a3b8' }}
          />
          <Legend wrapperStyle={{ fontSize: 11, color: '#64748b' }} />
          {lines.map(l => (
            <Line
              key={l.key as string}
              type="monotone"
              dataKey={l.key as string}
              stroke={l.color}
              name={l.label}
              dot={false}
              strokeWidth={2}
              isAnimationActive={false}
            />
          ))}
        </LineChart>
      </ResponsiveContainer>
    </div>
  )
}
