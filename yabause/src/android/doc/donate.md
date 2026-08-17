Test pattern

1. donate $3, 起動: サポートが消えていることを確認
1. アンインストール, インストール: サポートが出ていることを確認
1. donate $5, 起動: サポートが消えていることを確認
1. アンインストール, インストール: サポートが消えていることを確認
1. アンインストール, インストール: サポートが消えていることを確認
1. アンインストール, インストール: サポートが消えていることを確認
1. アンインストール, インストール: サポートが消えていることを確認
1. アンインストール, インストール: サポートが出ていることを確認
1. donate $10, 起動: サポートが消えていることを確認
1. アンインストール, インストール: サポートが消えていることを確認
1. ほかのデバイスでインストール、起動 : サポートが消えていることを確認
1. $10 購入をキャンセル, 起動: サポートが出ていることを確認
1. donate $5, 起動: サポートが消えていることを確認
1. サーバー停止,起動: サポートが消えていることを確認
1. サーバー bad tokenを返却 : サポートが出ていることを確認
1. $5 購入をキャンセル, 起動: サポートが出ていることを確認
1. サーバー停止、donate $5, 起動: サポートが消えていることを確認
1. $5 購入をキャンセル, 起動: サポートが出ていることを確認
1. サーバー bad tokenを返却、donate $5, 起動: サポートが出ていることを確認
1. donate $5, 起動: サポートが消えていることを確認
1. アンインストール, サーバー bad tokenを返却, アプリ起動: サポートが出ていることを確認
1. アンインストール, サーバー停止, アプリ起動: サポートが消えていることを確認


```puml
title Donation
hide footbox


actor User as user
participant DonateActivity 
participant IabHelper 
participant SharedPreferences
participant Server
participant Google

user -> DonateActivity : Donate
DonateActivity -> DonateActivity : Generate payload
DonateActivity -> IabHelper : launchPurchaseFlow
IabHelper --> DonateActivity : OnIabPurchaseFinishedListener
DonateActivity -> Server : activate
Server -> Google : varidate
Google --> Server : result
alt 200
  Server --> DonateActivity : true
  activate DonateActivity
  DonateActivity -> SharedPreferences : setDonated True
  deactivate DonateActivity
else 400
  Server --> DonateActivity : bad toekn
  activate DonateActivity
  DonateActivity -> SharedPreferences : setDonated False
  deactivate DonateActivity
else 500 | else
  Server --> DonateActivity : Server Error
  activate DonateActivity
  DonateActivity -> SharedPreferences : setDonated True
  deactivate DonateActivity
end


```

```puml
title DonateCheck
hide footbox


actor User as user
participant GameSelectActivity 
participant IabHelper 
participant SharedPreferences
participant Server
participant Google

user -> GameSelectActivity : install
activate GameSelectActivity
GameSelectActivity -> GameSelectActivity : CheckDonated
create IabHelper
GameSelectActivity -> IabHelper : << new >>
activate IabHelper
GameSelectActivity -> IabHelper : startSetup
IabHelper -> GameSelectActivity : onIabSetupFinished
GameSelectActivity -> IabHelper : queryPurchases
GameSelectActivity <-- IabHelper :  Purchases
deactivate IabHelper
GameSelectActivity -> SharedPreferences : getBoolean donated
GameSelectActivity <-- SharedPreferences : result
alt not donated 
  
  GameSelectActivity -> GameSelectActivity : getPurchaseState

  alt payed
    GameSelectActivity -> SharedPreferences : setBoolean donated
    GameSelectActivity -> Server : GetStateItem
    activate Server
    Server --> GameSelectActivity : response
    deactivate Server
    alt not found
      GameSelectActivity -> GameSelectActivity : Activate
    else too much
      GameSelectActivity -> SharedPreferences : setBoolean not donated
      GameSelectActivity -> IabHelper : consumeAsync
      activate IabHelper
      GameSelectActivity <--> IabHelper : result
      deactivate IabHelper
    end
  end

else donated 
  GameSelectActivity -> Server : getStatus
  activate Server
  Server -> Google : varidate
  Google --> Server : result
  alt 200
    Server --> GameSelectActivity : true
    activate GameSelectActivity
    alt active
      GameSelectActivity -> SharedPreferences : setDonated True
    else canceled
      GameSelectActivity -> SharedPreferences : setDonated false
    end
    deactivate ameSelectActivity
  else 400
    Server --> GameSelectActivity : bad toekn
    activate GameSelectActivity
    GameSelectActivity -> SharedPreferences : setDonated False
    deactivate GameSelectActivity
  else 500 | else
    Server --> GameSelectActivity : Server Error
    activate GameSelectActivity
    GameSelectActivity -> SharedPreferences : setDonated True
    deactivate GameSelectActivity
  end
  deactivate Server
end


```

