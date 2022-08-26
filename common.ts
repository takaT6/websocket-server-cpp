const Const = {
    WS_ADDRESS: "wss://echo.websocket.org",
}
// ユーザーclass
class userClass {
    wsConection: WebSocket; // WebSocket
    status: number; // 0: 未コネ, 1: 待機, 2: ホスト, 3: ゲスト 
    hostExists: boolean; // ホストがそんざいするかどうか

    // コンストラクタ
    constructor() {
        // コネクションの確立を試行
        this.login();
        // サーバーの状態を確認
        this.checkServerStat();
    }
    
    wsOnOpenFunc = (): void => {
        this.status = 1; // コネ確立
        alert("conection is published!!!")
        console.log("conection is published!!!");
    }

    wsOnErrorFunc = (): void => {
        console.log('エラーが発生しました。');
    }

    wsOnMessageFunc = (event: any): void => {
        const jsonData = JSON.parse(String(event.data));
        // ホストになったことを受信
        if(typeof jsonData.isHost !== 'undefined'){
            this.status = jsonData.isHost ? 2 : 1; // 2:ホストor 1:待機
            alert("あなたはホストになりました。")
            return;
        }
        // ゲストになったことを受信
        if(typeof jsonData.isGuest !== 'undefined'){
            this.status = jsonData.isGuest ? 2 : 1; // 3:ゲストor 1:待機
            alert("あなたはゲストになりました。")
            return;
        }
        // ホストがいるかどうかを受信
        if(typeof jsonData.hostExists !== 'undefined'){
            this.hostExists = jsonData.hostExists; // T or F
            this.hostExists ? alert("ホストはまだいません。") : alert("ホストは存在します。");
            return; 
        }
        console.log(jsonData);
    }

    wsOnCloseFunc = () => {
        this.status = 0; // 未コネ
        console.log("conection is down!!!");
    }

    // コネクションの確立
    login = (): boolean => {
        if ('WebSocket' in window) {
            this.wsConection = new WebSocket(Const.WS_ADDRESS);
            this.wsConection.onopen = () => this.wsOnOpenFunc();
            this.wsConection.onerror = () => this.wsOnErrorFunc();
            this.wsConection.onmessage = (event: any) => this.wsOnMessageFunc(event);
            this.wsConection.onclose = () => this.wsOnCloseFunc();
            return true;
        } else {
            console.log('WebSocket NOT supported in this browser');
            return false;
        }
    }

    // サーバーの状態を確認する
    checkServerStat = () => {
        if( this.status > 0){
            this.wsConection.send('checkserver');
        }else{
            console.log("コネクションが確立していません。");
        }
    }

    // ホストをとる
    beHost = (): void => {
        if( (this.status == 1 || this.status == 3) && !this.hostExists){// 1:待機, 3:ゲスト 
            this.wsConection.send('beHost');
        }else{
            console.log("コネクションが確立していません。");
        }
    }

    // ゲストになる
    beGuest = (): void => {
        if( (this.status == 1 || this.status == 2) && !this.hostExists){// 1:待機, 2:ホスト 
            this.wsConection.send('beGuest');
        }else{
            console.log("コネクションが確立していません。");
        }
    }

    // コネクション切断
    logout = (): void => {
        this.wsConection.close();
    }

    
}

const user = new userClass();