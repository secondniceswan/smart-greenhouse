'use client'

import { useEffect, useState } from 'react'
import { supabase, SensorLog, Weather, describeWeather } from '@/lib/supabase'
import { LineChart, Line, XAxis, YAxis, Tooltip, ResponsiveContainer, CartesianGrid } from 'recharts'

export default function Page() {
  const [latest, setLatest] = useState<SensorLog | null>(null)
  const [history, setHistory] = useState<SensorLog[]>([])
  const [weather, setWeather] = useState<Weather | null>(null)
  const [mode, setMode] = useState<'auto' | 'manual'>('auto')
  const [fanMode, setFanMode] = useState<'auto' | 'manual'>('auto')
  const [pendingRoof, setPendingRoof] = useState<'open' | 'closed' | null>(null)
  const [pendingFan, setPendingFan] = useState<boolean | null>(null)
  const [doneRoof, setDoneRoof] = useState(false)
  const [doneFan, setDoneFan] = useState(false)
  const [now, setNow] = useState(Date.now())
  const [sending, setSending] = useState(false)

  useEffect(() => {
    loadLatest()
    loadHistory()
    loadWeather()

    const sensorCh = supabase
      .channel('sensor_logs')
      .on(
        'postgres_changes',
        { event: 'INSERT', schema: 'public', table: 'sensor_logs' },
        (payload) => {
          const row = payload.new as SensorLog
          setLatest(row)
          setHistory((prev) => [...prev, row].slice(-288))

          setPendingRoof((p) => {
            if (p && row.roof_state === p) {
              setDoneRoof(true)
              setTimeout(() => setDoneRoof(false), 2000)
              return null
            }
            return p
          })
          setPendingFan((p) => {
            if (p !== null && row.fan_state === p) {
              setDoneFan(true)
              setTimeout(() => setDoneFan(false), 2000)
              return null
            }
            return p
          })
        }
      )
      .subscribe()

    const weatherCh = supabase
      .channel('weather')
      .on(
        'postgres_changes',
        { event: 'UPDATE', schema: 'public', table: 'weather' },
        (payload) => setWeather(payload.new as Weather)
      )
      .subscribe()

    const tick = setInterval(() => setNow(Date.now()), 10000)

    return () => {
      supabase.removeChannel(sensorCh)
      supabase.removeChannel(weatherCh)
      clearInterval(tick)
    }
  }, [])

  async function loadLatest() {
    const { data } = await supabase
      .from('sensor_logs')
      .select('*')
      .order('created_at', { ascending: false })
      .limit(1)
    if (data?.[0]) {
      setLatest(data[0])
      setMode(data[0].mode === 'manual' ? 'manual' : 'auto')
      setFanMode(data[0].fan_mode === 'manual' ? 'manual' : 'auto')
    }
  }

  async function loadHistory() {
    const since = new Date(Date.now() - 24 * 3600 * 1000).toISOString()
    const { data } = await supabase
      .from('sensor_logs')
      .select('*')
      .gte('created_at', since)
      .order('created_at', { ascending: true })
    if (data) setHistory(data)
  }

  async function loadWeather() {
    const { data } = await supabase.from('weather').select('*').eq('id', 1).single()
    if (data) setWeather(data as Weather)
  }

  async function sendCommand(action: 'open' | 'close' | 'auto') {
    setSending(true)
    await supabase.from('commands').insert({ action, executed: false })
    setMode(action === 'auto' ? 'auto' : 'manual')
    if (action === 'open') setPendingRoof('open')
    else if (action === 'close') setPendingRoof('closed')
    setSending(false)
  }

  async function sendFanCommand(action: 'fan_on' | 'fan_off' | 'fan_auto') {
    setSending(true)
    await supabase.from('commands').insert({ action, executed: false })
    setFanMode(action === 'fan_auto' ? 'auto' : 'manual')
    if (action === 'fan_on') setPendingFan(true)
    else if (action === 'fan_off') setPendingFan(false)
    setSending(false)
  }

  const lastUpdateText = latest ? relativeTime(now - new Date(latest.created_at).getTime()) : 'Belum ada'

  const chartData = history.map((h) => ({
    time: new Date(h.created_at).toLocaleTimeString('id-ID', { hour: '2-digit', minute: '2-digit' }),
    suhu: h.temperature,
    kelembapan: h.humidity,
  }))

  return (
    <main className="min-h-screen px-6 py-8 md:px-10 md:py-12 max-w-6xl mx-auto">
      <header className="flex items-center justify-between mb-10">
        <div>
          <h1 className="text-2xl font-semibold tracking-tight">Smart Greenhouse Dashboard</h1>
          <p className="text-sm text-zinc-500">Monitoring &amp; Kontrol</p>
        </div>
        <div className="text-right text-sm">
          <div className="text-zinc-400 text-xs">Update terakhir</div>
          <div className="text-zinc-700">{lastUpdateText}</div>
        </div>
      </header>

      <section className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-10">
        <Stat label="Suhu" value={fmt(latest?.temperature, 1)} unit="°C" />
        <Stat label="Kelembapan" value={fmt(latest?.humidity, 0)} unit="%" />
        <Stat label="Cahaya" value={fmt(latest?.lux, 0)} unit="lux" />
        <Stat label="Hujan" value={latest?.is_raining ? 'Ya' : 'Tidak'} />
      </section>

      <section className="grid md:grid-cols-3 gap-4 mb-10">
        <div className="bg-white rounded-xl border border-zinc-200 p-6">
          <div className="flex items-baseline justify-between mb-1">
            <h2 className="text-sm font-medium text-zinc-500">Ventilasi Atap</h2>
            <span className="text-xs text-zinc-400">
              {mode === 'auto' ? 'Otomatis' : 'Manual'}
            </span>
          </div>
          <div className="flex items-center gap-3 mb-2">
            <span className={`w-3 h-3 rounded-full ${latest?.roof_state === 'open' ? 'bg-emerald-500' : 'bg-zinc-300'}`} />
            <span className="text-3xl font-semibold">
              {latest?.roof_state === 'open' ? 'Buka' : 'Tutup'}
            </span>
          </div>
          <div className="text-xs h-4 mb-4">
            {pendingRoof ? (
              <span className="text-amber-600">Menunggu konfirmasi ESP...</span>
            ) : doneRoof ? (
              <span className="text-emerald-600">Selesai</span>
            ) : null}
          </div>

          <div className="grid grid-cols-3 gap-2">
            <button
              onClick={() => sendCommand('open')}
              disabled={sending || pendingRoof !== null}
              className="px-4 py-2 rounded-lg bg-zinc-900 text-white text-sm font-medium hover:bg-zinc-800 disabled:opacity-50"
            >
              Buka
            </button>
            <button
              onClick={() => sendCommand('close')}
              disabled={sending || pendingRoof !== null}
              className="px-4 py-2 rounded-lg bg-zinc-900 text-white text-sm font-medium hover:bg-zinc-800 disabled:opacity-50"
            >
              Tutup
            </button>
            <button
              onClick={() => sendCommand('auto')}
              disabled={sending || mode === 'auto'}
              className="px-4 py-2 rounded-lg border border-zinc-300 text-sm font-medium hover:bg-zinc-50 disabled:opacity-50"
            >
              Otomatis
            </button>
          </div>
        </div>

        <div className="bg-white rounded-xl border border-zinc-200 p-6">
          <div className="flex items-baseline justify-between mb-1">
            <h2 className="text-sm font-medium text-zinc-500">Kipas</h2>
            <span className="text-xs text-zinc-400">
              {fanMode === 'auto' ? 'Otomatis' : 'Manual'}
            </span>
          </div>
          <div className="flex items-center gap-3 mb-2">
            <span className={`w-3 h-3 rounded-full ${latest?.fan_state ? 'bg-emerald-500' : 'bg-zinc-300'}`} />
            <span className="text-3xl font-semibold">
              {latest?.fan_state ? 'Nyala' : 'Mati'}
            </span>
          </div>
          <div className="text-xs h-4 mb-4">
            {pendingFan !== null ? (
              <span className="text-amber-600">Menunggu konfirmasi ESP...</span>
            ) : doneFan ? (
              <span className="text-emerald-600">Selesai</span>
            ) : null}
          </div>

          <div className="grid grid-cols-3 gap-2">
            <button
              onClick={() => sendFanCommand('fan_on')}
              disabled={sending || pendingFan !== null}
              className="px-4 py-2 rounded-lg bg-zinc-900 text-white text-sm font-medium hover:bg-zinc-800 disabled:opacity-50"
            >
              Nyala
            </button>
            <button
              onClick={() => sendFanCommand('fan_off')}
              disabled={sending || pendingFan !== null}
              className="px-4 py-2 rounded-lg bg-zinc-900 text-white text-sm font-medium hover:bg-zinc-800 disabled:opacity-50"
            >
              Mati
            </button>
            <button
              onClick={() => sendFanCommand('fan_auto')}
              disabled={sending || fanMode === 'auto'}
              className="px-4 py-2 rounded-lg border border-zinc-300 text-sm font-medium hover:bg-zinc-50 disabled:opacity-50"
            >
              Otomatis
            </button>
          </div>
        </div>

        <div className="bg-white rounded-xl border border-zinc-200 p-6">
          <div className="flex items-baseline justify-between mb-4">
            <h2 className="text-sm font-medium text-zinc-500">Prediksi Cuaca</h2>
            <span className="text-xs text-zinc-400">{weather?.hour_label ?? ''}</span>
          </div>
          {weather ? (
            <div className="space-y-3">
              <div>
                <div className="text-xs text-zinc-500">Kondisi</div>
                <div className="text-xl font-medium">{describeWeather(weather.weather_code)}</div>
              </div>
              <div>
                <div className="text-xs text-zinc-500">Probabilitas Hujan</div>
                <div className="text-3xl font-semibold tabular-nums">{weather.rain_probability ?? 0}%</div>
              </div>
              <div className="grid grid-cols-2 gap-3 pt-1">
                <div>
                  <div className="text-xs text-zinc-500">Suhu</div>
                  <div className="text-base tabular-nums">{fmt(weather.temperature, 1)} °C</div>
                </div>
                <div>
                  <div className="text-xs text-zinc-500">Kelembapan</div>
                  <div className="text-base tabular-nums">{fmt(weather.humidity, 0)} %</div>
                </div>
              </div>
              <div className="text-xs text-zinc-400 pt-2 border-t border-zinc-100">
                Jatinangor · Open-Meteo
              </div>
            </div>
          ) : (
            <div className="text-sm text-zinc-400">Belum ada data</div>
          )}
        </div>
      </section>

      <section className="bg-white rounded-xl border border-zinc-200 p-6 mb-10">
        <h2 className="text-sm font-medium text-zinc-500 mb-4">Suhu &amp; Kelembapan 24 Jam</h2>
        <div className="h-64">
          {chartData.length > 0 ? (
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={chartData}>
                <CartesianGrid stroke="#f4f4f5" vertical={false} />
                <XAxis dataKey="time" stroke="#a1a1aa" fontSize={11} tickLine={false} axisLine={false} minTickGap={40} />
                <YAxis stroke="#a1a1aa" fontSize={11} tickLine={false} axisLine={false} />
                <Tooltip contentStyle={{ borderRadius: 8, border: '1px solid #e4e4e7', fontSize: 12 }} />
                <Line type="monotone" dataKey="suhu" stroke="#059669" strokeWidth={2} dot={false} />
                <Line type="monotone" dataKey="kelembapan" stroke="#0284c7" strokeWidth={2} dot={false} />
              </LineChart>
            </ResponsiveContainer>
          ) : (
            <div className="h-full flex items-center justify-center text-sm text-zinc-400">
              Belum ada data
            </div>
          )}
        </div>
        <div className="flex gap-4 mt-3 text-xs text-zinc-500">
          <span className="flex items-center gap-1.5">
            <span className="w-2 h-2 rounded-full bg-emerald-600" /> Suhu (°C)
          </span>
          <span className="flex items-center gap-1.5">
            <span className="w-2 h-2 rounded-full bg-sky-600" /> Kelembapan (%)
          </span>
        </div>
      </section>

      {latest?.overheating && (
        <div className="bg-amber-50 border border-amber-200 text-amber-900 rounded-xl px-4 py-3 text-sm">
          Peringatan anti-oven: suhu di dalam greenhouse tinggi saat atap tertutup.
        </div>
      )}

      <footer className="text-xs text-zinc-400 text-center mt-12">
        Update terakhir: {latest ? new Date(latest.created_at).toLocaleString('id-ID') : '-'}
      </footer>
    </main>
  )
}

function Stat({ label, value, unit }: { label: string; value: string; unit?: string }) {
  return (
    <div className="bg-white rounded-xl border border-zinc-200 p-5">
      <div className="text-xs text-zinc-500 mb-1">{label}</div>
      <div className="flex items-baseline gap-1">
        <span className="text-2xl font-semibold tabular-nums">{value}</span>
        {unit && <span className="text-sm text-zinc-400">{unit}</span>}
      </div>
    </div>
  )
}

function fmt(n: number | null | undefined, digits = 1) {
  if (n == null || isNaN(n) || n < 0) return '—'
  return n.toFixed(digits)
}

function relativeTime(ms: number) {
  if (ms < 0) ms = 0
  const sec = Math.floor(ms / 1000)
  if (sec < 60) return `${sec} detik lalu`
  const min = Math.floor(sec / 60)
  if (min < 60) return `${min} menit lalu`
  const hr = Math.floor(min / 60)
  if (hr < 24) return `${hr} jam lalu`
  const day = Math.floor(hr / 24)
  return `${day} hari lalu`
}
