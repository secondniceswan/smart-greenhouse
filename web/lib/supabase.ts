import { createClient } from '@supabase/supabase-js'

export const supabase = createClient(
  process.env.NEXT_PUBLIC_SUPABASE_URL!,
  process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY!
)

export type SensorLog = {
  id: number
  created_at: string
  temperature: number
  humidity: number
  lux: number
  is_raining: boolean
  roof_angle: number
  roof_state: 'open' | 'closed'
  mode: string
  overheating: boolean
}

export type Command = {
  action: 'open' | 'close' | 'auto'
  executed: boolean
}

export type Weather = {
  id: number
  updated_at: string
  hour_label: string
  rain_probability: number
  temperature: number
  humidity: number
  weather_code: number
}

export function describeWeather(code: number | null | undefined): string {
  if (code == null) return '—'
  if (code === 0) return 'Cerah'
  if (code <= 3) return 'Berawan'
  if (code === 45 || code === 48) return 'Berkabut'
  if (code >= 51 && code <= 57) return 'Gerimis'
  if (code >= 61 && code <= 67) return 'Hujan'
  if (code >= 80 && code <= 82) return 'Hujan Deras'
  if (code >= 95) return 'Badai Petir'
  return '—'
}
