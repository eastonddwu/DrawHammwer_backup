-- dbproxy MySQL 后端表结构
-- 对应 tb_app_tcaplus.xml 中的 login / user_info 两张表，
-- 字段与TDR结构体（table/tb_app_tcaplus.h）保持一致，
-- 额外增加 data_version 列用于复刻tcaplus REPLACE_REQ的乐观锁语义。
-- 使用前请先创建数据库并授权，例如：
--   CREATE DATABASE app_server CHARACTER SET utf8mb4;
--   CREATE USER 'app_server'@'%' IDENTIFIED BY '<password>';
--   GRANT ALL PRIVILEGES ON app_server.* TO 'app_server'@'%';
-- 并将实际host/port/user/password/database填入 server/dbproxy/conf/mysql.conf

CREATE TABLE IF NOT EXISTS login (
    gid           BIGINT UNSIGNED NOT NULL PRIMARY KEY,   -- 对应 ullGid，玩家ID
    login_flag    INT UNSIGNED    NOT NULL DEFAULT 0,     -- 对应 dwLogin_flag，登录标记
    data_version  INT             NOT NULL DEFAULT 0      -- 乐观锁版本号，dbproxy内部维护
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS user_info (
    gid           BIGINT UNSIGNED NOT NULL PRIMARY KEY,   -- 对应 ullGid，玩家ID
    is_new        INT UNSIGNED    NOT NULL DEFAULT 0,     -- 对应 dwIs_new
    role_type     INT UNSIGNED    NOT NULL DEFAULT 0,     -- 对应 dwRole_type
    user_name     VARCHAR(128)    NOT NULL DEFAULT '',    -- 对应 szUser_name[128]
    points        BIGINT UNSIGNED NOT NULL DEFAULT 0,     -- 对应 ullPoints
    data_version  INT             NOT NULL DEFAULT 0      -- 乐观锁版本号，dbproxy内部维护
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
