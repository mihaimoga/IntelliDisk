DROP TABLE IF EXISTS `filedata`;
DROP TABLE IF EXISTS `filename`;
CREATE TABLE `filename` (`filename_id` BIGINT NOT NULL AUTO_INCREMENT, `filepath` VARCHAR(256) NOT NULL, `filesize` BIGINT NOT NULL, PRIMARY KEY(`filename_id`)) ENGINE=InnoDB;
CREATE TABLE `filedata` (`filedata_id` BIGINT NOT NULL AUTO_INCREMENT, `filename_id` BIGINT NOT NULL, `content` LONGTEXT NOT NULL, `base64` BIGINT NOT NULL, PRIMARY KEY(`filedata_id`), FOREIGN KEY filedata_fk(filename_id) REFERENCES filename(filename_id)) ENGINE=InnoDB;
CREATE UNIQUE INDEX index_filepath ON `filename`(`filepath`);
