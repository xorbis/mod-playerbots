-- XorWoW: bots crowd control mind controlled group members by default; bots with saved strategies replace the config defaults on login
UPDATE `playerbots_db_store` SET `value` = CONCAT(`value`, ',+cc mind control')
WHERE `key` = 'co' AND `value` NOT LIKE '%cc mind control%';
