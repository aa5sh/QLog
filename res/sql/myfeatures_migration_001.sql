CREATE TABLE IF NOT EXISTS myfeatures_schema_versions (
    version INTEGER PRIMARY KEY,
    updated TEXT NOT NULL
);
ALTER TABLE alert_rules ADD COLUMN dx_marathon INTEGER DEFAULT 0;
