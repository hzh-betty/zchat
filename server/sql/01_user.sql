USE `zchat`;

CREATE TABLE IF NOT EXISTS `user` (
  `id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
  `user_id` varchar(64) NOT NULL,
  `nickname` varchar(64) NULL,
  `description` TEXT NULL,
  `password` varchar(255) NULL,
  `password_hash_algo` varchar(16) NOT NULL DEFAULT 'argon2id',
  `login_fail_count` INT NOT NULL DEFAULT 0,
  `lock_until` TIMESTAMP NULL DEFAULT NULL,
  `phone` varchar(64) NULL,
  `avatar_id` varchar(64) NULL,
  UNIQUE KEY `user_id_i` (`user_id`),
  UNIQUE KEY `nickname_i` (`nickname`),
  UNIQUE KEY `phone_i` (`phone`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
