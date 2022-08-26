var Const = {
    WS_ADDRESS: "wss://echo.websocket.org"
};
// ユーザーclass
var userClass = /** @class */ (function () {
    // コンストラクタ
    function userClass() {
        var _this = this;
        this.wsOnOpenFunc = function () {
            _this.status = 1; // コネ確立
            console.log("conection is published!!!");
        };
        this.wsOnErrorFunc = function () {
            console.log('エラーが発生しました。');
        };
        this.wsOnMessageFunc = function (event) {
            var jsonData = JSON.parse(String(event.data));
            if (typeof jsonData.isHost !== 'undefined') {
                _this.status = jsonData.isHost ? 2 : 3; // ホスト：ゲスト
            }
            console.log(jsonData);
        };
        this.wsOnCloseFunc = function () {
            _this.status = 0; // 未コネ
            console.log("conection is down!!!");
        };
        // コネクションの
        this.makeConnection = function () {
            if ('WebSocket' in window) {
                _this.wsConection = new WebSocket(Const.WS_ADDRESS);
                _this.wsConection.onopen = function () { return _this.wsOnOpenFunc(); };
                _this.wsConection.onerror = function () { return _this.wsOnErrorFunc(); };
                _this.wsConection.onmessage = function (event) { return _this.wsOnMessageFunc(event); };
                _this.wsConection.onclose = function () { return _this.wsOnCloseFunc(); };
                return true;
            }
            else {
                console.log('WebSocket NOT supported in this browser');
                return false;
            }
        };
        // ホストをとる
        this.beHost = function () {
            if (_this.status) {
                _this.wsConection.send('checkin');
            }
        };
        //コネクションの確立を試行
        this.makeConnection();
    }
    return userClass;
}());
var user = new userClass();
