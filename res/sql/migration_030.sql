CREATE TABLE IF NOT EXISTS activity_profiles(
        profile_name TEXT PRIMARY KEY,
        config TEXT,
        selected INTEGER
);

INSERT INTO activity_profiles
SELECT profile_name, json_object(
           'activityName', profile_name,
           'profiles', json_array(
               json_object(
                   'profileType', 4,
                   'name', profile_name
               )
           )
       ) AS config,
       selected
FROM main_layout_profiles;


ALTER TABLE alert_rules ADD COLUMN ituz INTEGER;
ALTER TABLE alert_rules ADD COLUMN cqz INTEGER;

ALTER TABLE main_layout_profiles ADD tabsexpanded INTEGER DEFAULT 1;

ALTER TABLE station_profiles ADD county TEXT;
ALTER TABLE station_profiles ADD operator_callsign TEXT;
