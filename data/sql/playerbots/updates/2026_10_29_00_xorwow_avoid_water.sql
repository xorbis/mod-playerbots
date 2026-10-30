-- XorWoW: bots go around water by default; bots with saved strategies replace the config defaults on login
UPDATE `playerbots_db_store` SET `value` = CONCAT(`value`, ',+avoid water')
WHERE `key` IN ('co', 'nc') AND `value` NOT LIKE '%avoid water%';
