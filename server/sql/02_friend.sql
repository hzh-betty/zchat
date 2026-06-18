USE `zchat`;

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
