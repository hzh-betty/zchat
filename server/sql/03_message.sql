USE `zchat`;

CREATE TABLE IF NOT EXISTS `message` (
  `id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
  `message_id` varchar(64) NOT NULL,
  `session_id` varchar(64) NOT NULL,
  `user_id` varchar(64) NOT NULL,
  `message_type` TINYINT UNSIGNED NOT NULL,
  `create_time` DATETIME(3) NULL DEFAULT CURRENT_TIMESTAMP(3),
  `content` MEDIUMTEXT NULL,
  `file_id` varchar(64) NULL,
  `file_name` varchar(128) NULL,
  `file_size` BIGINT UNSIGNED NULL,
  UNIQUE KEY `message_id_i` (`message_id`),
  KEY `session_id_i` (`session_id`),
  KEY `message_time_i` (`session_id`, `create_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
