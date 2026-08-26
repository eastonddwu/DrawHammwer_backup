# Domain 配置文件说明

## 基本说明

- 该文件决定了整个集群的路由配置，包括 busid 格式、group 属性、alias 规则，等等
- nameserver 支持 domain.yaml 或 domain.json 两种格式的配置文件
- 所有字段若无特殊说明，均支持动态调整（reload）

## 各字段配置规范

### version

- 当前配置的版本，uint32 类型

### busid_template

- 整个集群中 busid 的格式模板，格式如 [bitSize#]a[.b.c...].*
- bitSize：busid 的总位数，高8位被系统保留，因此可设定的范围为 [32, 56]，默认为 32 位
- a[.b.c...]：busid 各分段位数，最小分段数为2，不同分段可能对应 WorldId、ZoneId、ModuleId 等等，由用户根据需要灵活定义
- '\*' 符号：已显式配置的分段用于划分 group, 剩余位数作为最后一个分段，表示 endpoint 的组内编号，例如 "40#8.8.8.*" 表示 gid_mask 为 0xFFFFFF0000
- 示例："32#8.8.*" 表示 busid 的总位数为32位，分为3个分段，gid_mask 自动推导为 0xFFFF0000, endpoint 的组内编号共16位

### gid_mask

- 此掩码用于确定 busid_template 中的哪些分段用于划分 group，哪些分段用于表示 endpoint 的组内编号，gid = busid & gid_mask
- 遵循十六进制表示方式，格式如 0xFFFF0000，划分时最小单位为段，即如果 busid_template 为 "32#8.8.8.8"，则 gid_mask 不能设置为 0xFFFF**F0**00
- 若 busid_template 使用了通配符号 "\*"，则 gid_mask 以其自动推导得到的为准
- reload: busid_template & gid_mask 均不支持，如需调整，必须停服重启

### groups

- 此列表配置了不同 group 的路由属性，每个 group 包含以下可配置的属性字段：
- group_id: 群组的 gid, 需符合 busid_template 模板，支持[通配规则](#特殊字段通配规则)，方便用户进行批量配置
- alias: 群组的别名，支持[通配规则](#特殊字段通配规则)，alias & group_id 两个字段必须填写其中一个
- desc: 描述信息，方便用户添加注释说明

- route_types: 支持的路由策略
  - 目前可选的策略有：ROUTE_TYPE_RANDOM(1), ROUTE_TYPE_M_HASH(3), ROUTE_TYPE_C_HASH(4), ROUTE_TYPE_MASTER(5)
  - 可以配置为枚举名/枚举值，也可以配置多个路由策略，第一项作为默认路由策略，在 api 发包未指定路由策略时使用
  - 当存在 ROUTE_TYPE_M_HASH、ROUTE_TYPE_C_HASH、ROUTE_TYPE_MASTER 中任意一个策略时，该群组视为有状态组
  ``` yaml
  ROUTE_TYPE_RANDOM   1   // 随机
  ROUTE_TYPE_M_HASH   3   // 取模哈希
  ROUTE_TYPE_C_HASH   4   // 一致性哈希
  ROUTE_TYPE_MASTER   5   // 选主
  ```

- allow_msg_wrong_version: 当群组是有状态组时，是否允许接收 group_version 不匹配的消息，默认不允许
- route_c_hash_replica: 哈希环虚拟节点个数，默认值为 100，最大值为 10000，仅当路由策略包含 ROUTE_TYPE_C_HASH 时有效

- gray_rules: 灰度规则（原跳线规则）
  - 如果消息来源的 busid 或者消息携带的 hash_key 或者源 shard_id 符合匹配规则（支持[通配规则](#特殊字段通配规则)），则直接转发至指定的目标，或者修改选路的规则
  - 可用于有状态组在灰度更新的过程中，实现流量的灰度切换
  ``` yaml
  # 匹配规则可以设置多种，转发规则设置其中一种
  gray_rules:
      # 匹配规则
    - src_busid_spec: 1.0.0.2
      hash_key_spec: '1000,1200-1210'
      src_shard_id_spec: '3,5-10'
      # 转发给指定的目标节点
      dest_busid: 2.0.0.1
      # 在指定的目标子群内选路
      dest_shard_id: 4
      # 在指定的目标版本范围内选路，仅对 P2G 随机选路有效
      dest_min_version: 1
      dest_max_version: 3
      # 修改消息的路由策略，仅对 P2G 选路有效
      dest_route_type: 1
  ```

- shard_weights: 子群权重规则
  - 当 api 发包设置 shard_id = 0 时，选路将在所有在线子群中按照权重进行分发
  - 子群权重范围：[0, 100]，如果某个在线子群未显式配置权重，默认值为 100
  - 可用于无状态组在灰度更新的过程中，实现流量的灰度切换
  ``` yaml
  # shard1 命中概率 33%, shard2 命中概率 66%
  shard_weights:
    - shard_id: 1
      weight: 20
    - shard_id: 2
      weight: 40
  ```

- shard_route_policy: 子群寻路策略
  - 目前可选的策略有：GROUP_SHARDING_SAFE(0), GROUP_SHARDING_STRICT(1), GROUP_SHARDING_OFF(2)
  - 可以配置为枚举名/枚举值，该配置仅在 route_types 中存在哈希/选主策略时有效，作用于 P2G 单播寻路
  ``` yaml
  GROUP_SHARDING_SAFE   0   // 启用，若 shard_id 不存在，则全组匹配
  GROUP_SHARDING_STRICT 1   // 严格匹配 shard_id，不存在报错
  GROUP_SHARDING_OFF    2   // 取消 shard_id 匹配，默认全组匹配
  ```

### alias_rules

- 当 api 用形如 a.b.c.d 的 alias 注册实例时，nameserver 会拆分成多个分段单独分配 id，从而确定最终的 busid
- 此配置确定每个分段下的 alias 分配规则，每个分段包含以下字段：
- level: 分段的索引，从左到右，从1开始递增
- desc: 分段的描述信息
- min_autogen_id: 自动为别名分配 id 的最小值
- max_autogen_id: 自动为别名分配 id 的最大值，不允许超过该分段的位数上限
- alias: 固定的别名分配规则列表，支持在一行中用 ‘;’ 分隔符来配置多个规则，例如 "alias1=100;alias2=200"
- 注意: 自动分配的 id 范围不允许和固定分配的 id 发生冲突，系统在启动/重载时会做校验
- 如果某个分段的内容为 ‘\*’ 或者为空，系统会默认分配 id = 0，例如：alias a.b.\*.\* -> 150.150.0.0
- 如果某个分段的内容为数字，系统会直接保留数字的值，例如：alias a.b.3.4 -> busid 150.150.3.4
- 注意: 在处理数字分段时，如果该数字超过对应分段的位数限制，或者和分配规则发生冲突，会直接导致实例注册失败
- reload: 不支持调整自动分配范围，此外 nameserver 会校验新旧别名规则是否冲突，以及新别名规则是否和已有的别名信息冲突

### domain_alias_rules

- 指定 domain_id 对应的 alias，配置后在部署 agent 的时候可以用 alias 来设置 domain_id 参数
- 该配置和 busid 无关，属于 domain_id 子域隔离特性的扩展
- domain_id 允许范围为 [1, 255]

### use_16bits_domain_id

- domain_id 扩展到 16 bits，允许范围变为 [1, 65535]，默认情况下只支持 8 bits
- 启用该功能后，busid_template 最高只允许配置 48 bits，系统在启动时会做校验
- 该配置项会同时作用于子域隔离 & 跨域通讯特性的 domain_id 字段
- 注意：使用跨域通讯特性时，如需启用该功能，请确保 api & agent & router 全都先升级到支持的版本
- reload: 不支持，如需调整，必须停服重启

### router

- 跨集群转发的相关配置，目前只有 proxy_gid 字段，不使用该功能的业务无需配置
- proxy_gid: 当前集群的跨域消息转发的出口，对应 tbus2_router 配置的 proxy_busid 参数
- reload: 不支持

## 特殊字段通配规则

### 重要说明

- 在 yaml 格式的配置文件中使用特殊符号（如星号、逗号、中括号等）时，最好以双引号包裹以明确作为字符串解析

### groups/group_id

- 非通配: 1.5.0
- '\*' 通配：1.\*.0 -> 1.0.0, 1.1.0, ..., 1.255.0
- 线段树通配：1.[1-2,4,6-8].0 -> 1.1.0, 1.2.0, 1.4.0, 1.6.0, 1.7.0, 1.8.0
- 匹配优先级：从左往右开始比较每个分段的合法值数量，数量越少，则整个表达式的优先级越高，数量相等，则继续比较后续分段
- 优先级举例：1.5.0 > 1.[1-10].0 > 1.[1-20].0 > 1.\*.0 > [1-10].5.0

### groups/alias

- 非通配：game.shanghai.ins
- '\*' 通配：game.\*.\* -> game.beijing.ins1, game.taiyuan.ins2, game.gansu.ins3, ...
- 线段树通配：[game1,game2].city[1-3]zone[1-3].ins -> game1.city1zone1.ins, game1.city2zone3.ins, game2.city3zone1.ins, ...
- 对于 alias 的通配格式，允许在单个分段中拼接字符串，以实现更加灵活的配置

### groups/gray_rules/src_busid_spec

- 非通配：1.0.0.1（single busid）
- '\*' 通配：1.0.0.\* -> 1.0.0.1, 1.0.0.2, ..., 1.0.0.255
- 线段树通配：1.0.0.[8-10,15] -> 1.0.0.8, 1.0.0.9, 1.0.0.10, 1.0.0.15

### groups/gray_rules/hash_key_spec

- 非通配：12345（single hash key）
- 线段树通配："a,b,c-d" -> a, b, [c, d]
- 由于没有点分格式的限制，所以 hash_key_spec 的线段树通配格式和其他字段不一样