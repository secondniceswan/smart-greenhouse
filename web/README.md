# AGRIOS Dashboard

## Setup

```bash
cd web
npm install
cp .env.local.example .env.local
```

Isi `.env.local` dengan kredensial Supabase:

```
NEXT_PUBLIC_SUPABASE_URL=https://xxxxxxxx.supabase.co
NEXT_PUBLIC_SUPABASE_ANON_KEY=eyJhbGci...
```

## Jalankan Lokal

```bash
npm run dev
```

Buka http://localhost:3000

## Deploy ke Vercel

1. Push folder ini ke GitHub
2. Buka https://vercel.com → New Project → Import repo
3. Set Root Directory ke `web` (kalau folder ini di dalam repo lain)
4. Add Environment Variables: `NEXT_PUBLIC_SUPABASE_URL` dan `NEXT_PUBLIC_SUPABASE_ANON_KEY`
5. Deploy

## Tabel Supabase

```sql
CREATE TABLE sensor_logs (
  id BIGSERIAL PRIMARY KEY,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  temperature REAL,
  humidity REAL,
  lux REAL,
  is_raining BOOLEAN,
  roof_angle INT,
  roof_state TEXT,
  mode TEXT,
  overheating BOOLEAN,
  fan_state BOOLEAN,
  fan_mode TEXT
);

CREATE TABLE commands (
  id BIGSERIAL PRIMARY KEY,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  action TEXT,
  executed BOOLEAN DEFAULT FALSE
);

CREATE TABLE weather (
  id INT PRIMARY KEY,
  updated_at TIMESTAMPTZ DEFAULT NOW(),
  hour_label TEXT,
  rain_probability INT,
  temperature REAL,
  humidity REAL,
  weather_code INT
);

INSERT INTO weather (id, rain_probability, temperature) VALUES (1, 0, 0);

ALTER TABLE sensor_logs ENABLE ROW LEVEL SECURITY;
ALTER TABLE commands ENABLE ROW LEVEL SECURITY;
ALTER TABLE weather ENABLE ROW LEVEL SECURITY;

CREATE POLICY "read_all" ON sensor_logs FOR SELECT USING (true);
CREATE POLICY "insert_all" ON sensor_logs FOR INSERT WITH CHECK (true);
CREATE POLICY "read_cmd" ON commands FOR SELECT USING (true);
CREATE POLICY "insert_cmd" ON commands FOR INSERT WITH CHECK (true);
CREATE POLICY "update_cmd" ON commands FOR UPDATE USING (true);
CREATE POLICY "read_weather" ON weather FOR SELECT USING (true);
CREATE POLICY "update_weather" ON weather FOR UPDATE USING (true);
```

Aktifkan Realtime untuk tabel `sensor_logs` di Supabase Dashboard → Database → Replication.
