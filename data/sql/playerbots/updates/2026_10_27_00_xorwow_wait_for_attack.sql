-- XorWoW: bots wait before attacking by default; bots with saved strategies replace the config defaults on login
UPDATE `playerbots_db_store` SET `value` = CONCAT(`value`, ',+wait for attack')
WHERE `key` = 'co' AND `value` NOT LIKE '%wait for attack%';
