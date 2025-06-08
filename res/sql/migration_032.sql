UPDATE modes SET submodes = '["FMHELL", "FSKHELL", "FSKH105", "FSKH245", "HELL80", "HELLX5", "HELLX9", "HFSK", "PSKHELL", "SLOWHELL"]' WHERE name = 'HELL';

ALTER TABLE contacts ADD cnty_alt TEXT;
ALTER TABLE contacts ADD dcl_qslrdate TEXT;
ALTER TABLE contacts ADD dcl_qslsdate TEXT;
ALTER TABLE contacts ADD dcl_qsl_rcvd TEXT NOT NULL DEFAULT 'N';
ALTER TABLE contacts ADD dcl_qsl_sent TEXT NOT NULL DEFAULT 'N';
ALTER TABLE contacts ADD morse_key_info TEXT;
ALTER TABLE contacts ADD morse_key_type TEXT;
ALTER TABLE contacts ADD my_cnty_alt TEXT;
ALTER TABLE contacts ADD my_darc_dok TEXT;
ALTER TABLE contacts ADD my_morse_key_info TEXT;
ALTER TABLE contacts ADD my_morse_key_type TEXT;
ALTER TABLE contacts ADD qrzcom_qso_download_date TEXT;
ALTER TABLE contacts ADD qrzcom_qso_download_status TEXT;
ALTER TABLE contacts ADD qslmsg_rcvd TEXT;

ALTER TABLE station_profiles ADD darc_dok TEXT;

ALTER TABLE alert_rules ADD pota INTEGER DEFAULT 0;
ALTER TABLE alert_rules ADD sota INTEGER DEFAULT 0;
ALTER TABLE alert_rules ADD iota INTEGER DEFAULT 0;
ALTER TABLE alert_rules ADD wwff INTEGER DEFAULT 0;
