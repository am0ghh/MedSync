-- MedSync — Supabase setup
-- Run this in: Supabase Dashboard → SQL Editor → New query

-- Dose events table
CREATE TABLE IF NOT EXISTS dose_events (
  id             uuid        DEFAULT gen_random_uuid() PRIMARY KEY,
  day            text        NOT NULL,
  scheduled_time timestamptz NOT NULL,
  dispensed_at   timestamptz,
  status         text        NOT NULL DEFAULT 'pending',  -- pending | dispensed | missed
  photo_url      text,
  pills_detected boolean,    -- null until CV processor runs
  created_at     timestamptz DEFAULT now()
);

-- Remote commands table (caregiver app writes here; ESP32 polls it)
CREATE TABLE IF NOT EXISTS commands (
  id         uuid        DEFAULT gen_random_uuid() PRIMARY KEY,
  command    text        NOT NULL,  -- 'dispense' | 'load' | 'unload'
  executed   boolean     DEFAULT false,
  created_at timestamptz DEFAULT now()
);

-- Indexes for common queries
CREATE INDEX IF NOT EXISTS idx_dose_events_status      ON dose_events (status);
CREATE INDEX IF NOT EXISTS idx_dose_events_created_at  ON dose_events (created_at);
CREATE INDEX IF NOT EXISTS idx_commands_executed       ON commands (executed);

-- Cleanup is handled by the Python CV processor (no pg_cron needed).
