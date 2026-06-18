USE `zchat`;

CREATE TABLE IF NOT EXISTS `file_store` (
  `id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
  `file_id` varchar(64) NOT NULL,
  `file_name` varchar(128) NULL,
  `file_size` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `file_content` LONGBLOB NOT NULL,
  `owner_user_id` varchar(64) NULL,
  `chat_session_id` varchar(64) NULL,
  `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY `file_id_i` (`file_id`),
  KEY `owner_user_id_i` (`owner_user_id`),
  KEY `chat_session_id_i` (`chat_session_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
