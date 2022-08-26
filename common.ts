const Const = {
    WS_ADDRESS: "wss://echo.websocket.org",
}

// ユーザーclass
class userClass {
    private _wsConnection: WebSocket; // WebSocket
    private _userStatus: number; // 0: 未コネ, 1: 接続済, 2: ホスト, 3: ゲスト 
    private _hostExists: boolean; // ホストがそんざいするかどうか
    private _stopperConnection: WebSocket; // 停止用WebSocket
    private _stopperStatus: number; // 0: 未コネ, 1: 接続済 

    // コンストラクタ
    constructor() {
        this._userStatus = 0;
        this._stopperStatus = 0;
        this._hostExists = true;
        // コネクションの確立を試行
        this.login();
        // サーバーの状態を確認
        this.checkServerStat();
    }

    //getter
    get hostExists() {
        return this._hostExists;
    }

    //getter
    get userStatus() {
        return this._userStatus;
    }
    
    private wsOnOpenFunc = (): void => {
        this._userStatus = 1; // コネ確立
        alert("Connection is published!!!")
        console.log("Connection is published!!!");
    }

    private wsOnErrorFunc = (): void => {
        console.log('エラーが発生しました。');
    }

    private wsOnMessageFunc = (event: any): void => {
        const jsonData = JSON.parse(String(event.data));
        // ホストになったことを受信
        if(typeof jsonData.isHost !== 'undefined'){
            this._userStatus = jsonData.isHost ? 2 : 1; // 2:ホストor 1:接続済
            this.makeStopper();
            alert("あなたはホストになりました。")
            return;
        }
        // ゲストになったことを受信
        if(typeof jsonData.isGuest !== 'undefined'){
            this._userStatus = jsonData.isGuest ? 2 : 1; // 3:ゲストor 1:接続済
            alert("あなたはゲストになりました。")
            return;
        }
        // ホストがいるかどうかを受信
        if(typeof jsonData.hostExists !== 'undefined'){
            this._hostExists = jsonData.hostExists; // T or F
            this._hostExists ?  alert("ホストは存在します。"): alert("ホストはまだいません。");
            return; 
        }
        console.log(jsonData);
    }

    private wsOnCloseFunc = () => {
        this._userStatus = 0; // 未コネ
        console.log("Connection is down!!!");
    }

    // コネクションの確立
    private login = (): boolean => {
        if ('WebSocket' in window) {
            this._wsConnection = new WebSocket(Const.WS_ADDRESS);
            this._wsConnection.onopen = () => this.wsOnOpenFunc();
            this._wsConnection.onerror = () => this.wsOnErrorFunc();
            this._wsConnection.onmessage = (event: any) => this.wsOnMessageFunc(event);
            this._wsConnection.onclose = () => this.wsOnCloseFunc();
            return true;
        } else {
            console.log('WebSocket NOT supported in this browser');
            return false;
        }
    }

    // サーバーの状態を確認する
    public checkServerStat = () => {
        if( this._userStatus > 0){
            this._wsConnection.send('checkserver');
        }else{
            console.log("コネクションが確立していません。");
        }
    }

    // ホストをとる
    public beHost = (): void => {
        if( (this._userStatus == 1 || this._userStatus == 3) && !this._hostExists){// 1:接続済, 3:ゲスト 
            this._wsConnection.send('beHost');
        }else{
            console.log("コネクションが確立していません。");
        }
    }

    // ゲストになる
    public beGuest = (): void => {
        if( (this._userStatus == 1 || this._userStatus == 2) && !this._hostExists){// 1:接続済, 2:ホスト 
            this._wsConnection.send('beGuest');
        }else{
            console.log("コネクションが確立していません。");
        }
    }

    // コネクション切断
    public logout = (): void => {
        this._wsConnection.close();
    }

    // プロセスの停止
    public processStop = () => {
        if(this._userStatus == 2){
            if(this._stopperStatus == 1){
                this._stopperConnection.send('stop');
            }else if(this._stopperStatus == 0){
                this.makeStopper();
                this._stopperConnection.send('stop');
            }
        }
    }

    //ストッパーの作成
    private makeStopper = () => {
        if ('WebSocket' in window) {
            this._stopperConnection = new WebSocket(Const.WS_ADDRESS);
            this._stopperConnection.onopen = () => {
                this._stopperStatus = 1;
            };
            this._stopperConnection.onerror = () => {
                
            };
            this._stopperConnection.onmessage = (event: any) => {
                
            };
            this._stopperConnection.onclose = () => {
                this._stopperStatus = 0;
            };
            return true;
        } else {
            console.log('WebSocket NOT supported in this browser');
            return false;
        }
    }
}

const user = new userClass();