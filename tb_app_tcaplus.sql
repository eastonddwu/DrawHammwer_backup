CREATE DATABASE IF NOT EXISTS app_server DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE app_server;

-- 对应 LOGIN 结构体 (tb_app_tcaplus.h:121-123)
CREATE TABLE IF NOT EXISTS login (
    gid         BIGINT UNSIGNED NOT NULL,   -- ullGid
    login_flag  INT UNSIGNED NOT NULL DEFAULT 1,  -- dwLogin_flag
    PRIMARY KEY (gid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 对应 USER_INFO 结构体 (tb_app_tcaplus.h:126-132)
CREATE TABLE IF NOT EXISTS user_info (
    gid         BIGINT UNSIGNED NOT NULL,   -- ullGid
    is_new      INT UNSIGNED NOT NULL DEFAULT 0,   -- dwIs_new
    role_type   INT UNSIGNED NOT NULL DEFAULT 0,   -- dwRole_type
    user_name   VARCHAR(128) NOT NULL DEFAULT '',  -- szUser_name[128]
    points      BIGINT UNSIGNED NOT NULL DEFAULT 0, -- ullPoints
    PRIMARY KEY (gid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
