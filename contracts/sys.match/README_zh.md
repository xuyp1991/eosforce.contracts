# sys.match 合约说明文档

## 0. 合约介绍

sys.match是完成撮合交易的一个合约

## 1. 基础功能

### regex

regex 注册运营商

参数：

+ exc_acc 运营商帐户，account_name类型


```bash
./cleos push action sys.match regex '["eosforce"]' -p eosforce
```

对于撮合交易来说，运营商的角色十分重要，所有交易对由运营商创建，费用由运营商收取，合约中需要的内存由运营商支付。运营商创建之后又需要将自己的权限修改为合约可操作的权限
```bash
./cleos set account permission eosforce active '{ "threshold": 1, "keys": [ {"key":"EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV","weight":1} ], "accounts":[  { "permission": { "actor": "sys.match", "permission": "eosio.code" }, "weight": 1 } ] }' owner
```

### createtrade

createtrade 创建交易对

参数：

+ trade_pair_name  交易对名称
+ base_coin        作为商品的币种
+ quote_coin       作为货币的币种
+ exc_acc          运营商帐户名

```bash
./cleos push action sys.match createtrade '["eos.eat","0.0000 EOS","0.0000 EAT","eosforce"]' -p eosforce
```

创建交易对的时候base_coin作为商品，quote_coin代表货币，任何一单的成交均以商品的成交为基准。创建交易对的时候base_coin和quote_coin的数量也是交易对的最小数量

### feecreate

feecreate 创建费用收取规则

参数：

+ fee_name     费用规则的名称
+ fee_type     费用规则的类型
+ rate         成交时成交币种收取费用的比例
+ rate_base    成交时根据Base成交数量收取点卡的比例
+ rate_quote   成交时根据Quote成交数量收取点卡的比例
+ coin_card    收取点卡的币种
+ exc_acc      运营商帐户，account_name类型

```bash
./cleos push action sys.match feecreate '["rate1","p.ratio",100,100,10,"0.0000 EOS","eosforce"]' -p eosforce
```
fee_type的类型只有3种：
+ f.null       不收取费用
+ f.ratio      根据成交数量按比例收取
+ p.ratio      使用点卡，根据成交数量收取对应的点卡

采用p.ratio的收取方式，如果用户没有足够的点卡的话自动采用f.ratio的方式进行收取

### setfee

setfee 将费用的收取规则绑定到交易对上面

参数：

+ trade_pair_name    交易对的名称
+ fee_name           费用规则的名称
+ exc_acc            运营商帐户

```bash
./cleos setfee '["eos.eat","rate1","eosforce"]' -p eosforce
```

### opendeposit

使用撮合交易合约的用户需要在合约中由存单，用于存放代币以支付点卡费用，接收成交时成交的代币。

参数：

+ user          用户名
+ quantity      代币（只用到了symbol）
+ memo          备注

```bash
./cleos  push action sys.match opendeposit '["testa","0.0000 EAT","test"]' -p testa
```

### claimdeposit

在存单里面取代币，用户可使用该功能将代币从合约的存单里面领取出来。

参数：

+ user          用户名
+ quantity      领取的代币
+ memo          备注

```bash
./cleos  push action sys.match claimdeposit '["testa","1.0000 EAT","test"]' -p testa
```

### depositorder

depositorder提供用户通过在合约中的存单进行买卖挂单的功能

参数：

+ traders                     交易的帐户
+ base_coin                   支付的币
+ quote_coin                  要获得的币
+ trade_pair_name             交易对的名称
+ exc_acc                     运营商帐户

```bash
./cleos push action sys.match depositorder '["testa","10.0000 EOS","100.0000 EAT","eos.eat","eosforce"]' -p testa
```

### onforcetrans

监听eosio合约的transfer的Action，实现通过向合约帐户转帐挂单和向存单中充值等功能，参数和transfer的参数一样，通过解析memo实现相应功能

参数：

+ memo                  string类型，用;分割，第一个子串代表此次转帐的目的。

memo第一个子串的参数说明：

+ m.order               创建买卖单     例：m.order;1000.0000 EAT;eos.eat;eosforce   m.order代表这次转帐是要创建买卖单，1000.0000 EAT代币买卖单是要换得1000.0000 EAT，eos.eat是交易单名称，eosforce是运营商帐户
+ m.deposit             充值，给合约中的存单充值      例：m.deposit
+ m.donate              捐款，给合约帐户进行捐款      例：m.donate

```bash
./cleos  transfer testa sys.match "100.0000 EOS" "m.order;1000.0000 EAT;eos.eat;eosforce"
```

### ontokentrans

监听eosio.token合约的transfer的Action，实现通过向合约帐户转帐挂单和向存单中充值等功能，参数和transfer的参数一样，通过解析memo实现相应功能

参数：

+ memo                  string类型，用;分割，第一个子串代表此次转帐的目的。

memo第一个子串的参数说明：

+ m.order               创建买卖单     例：m.order;1000.0000 EAT;eos.eat;eosforce   m.order代表这次转帐是要创建买卖单，1000.0000 EAT代币买卖单是要换得1000.0000 EAT，eos.eat是交易单名称，eosforce是运营商帐户
+ m.deposit             充值，给合约中的存单充值      例：m.deposit
+ m.donate              捐款，给合约帐户进行捐款      例：m.donate

```bash
./cleos push action eosio.token transfer '["testb","sys.match","3500.0000 EAT","m.order;350.0000 EOS;eos.eat;eosforce"]' -p testb
```

## 表格说明：

+ exchanges             运营商表格，存放该合约中所有注册运营商的帐户，scope是合约帐户
+ pairs                 交易对表格，scope是创建该交易对的运营商
+ orderscope            交易单scope表格，存放交易单的scope
+ order                 买卖单表格，存放买卖单，scope从表格orderscope中获取
+ tradefee              费用表格，存放费用规则，scope是创建该交易对的运营商
+ deposit               存单表格，存放用户在该合约的存单，scope是用户帐户
+ deal                  成交表格，测试的时候会使用，正式上线会废弃
+ recorddeal            成交数量记录表格，测试的时候会使用，正式上线会废弃
+ recordprice           成交总量记录，测试的时候会使用，正式上线会废弃

