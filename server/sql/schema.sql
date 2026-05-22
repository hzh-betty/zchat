CREATE DATABASE IF NOT EXISTS `zchat` DEFAULT CHARACTER SET utf8mb4;
USE `zchat`;

CREATE TABLE IF NOT EXISTS `user` (
  `id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
  `user_id` varchar(64) NOT NULL,
  `nickname` varchar(64) NULL,
  `description` TEXT NULL,
  `password` varchar(64) NULL,
  `phone` varchar(64) NULL,
  `avatar_id` varchar(64) NULL,
  UNIQUE KEY `user_id_i` (`user_id`),
  UNIQUE KEY `nickname_i` (`nickname`),
  UNIQUE KEY `phone_i` (`phone`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `relation` (
  `id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
  `user_id` varchar(64) NOT NULL,
  `peer_id` varchar(64) NOT NULL,
  KEY `user_id_i` (`user_id`),
  UNIQUE KEY `relation_pair_i` (`user_id`, `peer_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `friend_apply` (
  `id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
  `event_id` varchar(64) NOT NULL,
  `user_id` varchar(64) NOT NULL,
  `peer_id` varchar(64) NOT NULL,
  UNIQUE KEY `event_id_i` (`event_id`),
  KEY `user_id_i` (`user_id`),
  KEY `peer_id_i` (`peer_id`),
  UNIQUE KEY `friend_apply_pair_i` (`user_id`, `peer_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `chat_session` (
  `id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
  `chat_session_id` varchar(64) NOT NULL,
  `chat_session_name` varchar(64) NOT NULL,
  `chat_session_type` tinyint NOT NULL,
  UNIQUE KEY `chat_session_id_i` (`chat_session_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `chat_session_member` (
  `id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
  `session_id` varchar(64) NOT NULL,
  `user_id` varchar(64) NOT NULL,
  KEY `session_id_i` (`session_id`),
  KEY `member_user_id_i` (`user_id`),
  UNIQUE KEY `session_member_i` (`session_id`, `user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `message` (
  `id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
  `message_id` varchar(64) NOT NULL,
  `session_id` varchar(64) NOT NULL,
  `user_id` varchar(64) NOT NULL,
  `message_type` TINYINT UNSIGNED NOT NULL,
  `create_time` TIMESTAMP NULL,
  `content` MEDIUMTEXT NULL,
  `file_id` varchar(64) NULL,
  `file_name` varchar(128) NULL,
  `file_size` INT UNSIGNED NULL,
  UNIQUE KEY `message_id_i` (`message_id`),
  KEY `session_id_i` (`session_id`),
  KEY `message_time_i` (`session_id`, `create_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `file_store` (
  `id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
  `file_id` varchar(64) NOT NULL,
  `file_name` varchar(128) NULL,
  `file_size` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `file_content` LONGBLOB NOT NULL,
  `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY `file_id_i` (`file_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
