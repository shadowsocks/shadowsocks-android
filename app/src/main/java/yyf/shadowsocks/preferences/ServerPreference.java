package yyf.shadowsocks.preferences;

/**
 * Created by yyf on 2015/6/11.
 */
public class ServerPreference {
    String server;
    int server_port;
    int local_port;
    String password;
    String method;
    String remarks;

    public String getServer() {
        return server;
    }

    public void setServer(String server) {
        this.server = server;
    }

    public String getMethod() {
        return method;
    }

    public void setMethod(String method) {
        this.method = method;
    }

    public int getServer_port() {
        return server_port;
    }

    public void setServer_port(int server_port) {
        this.server_port = server_port;
    }

    public int getLocal_port() {
        return local_port;
    }

    public void setLocal_port(int local_port) {
        this.local_port = local_port;
    }

    public String getPassword() {
        return password;
    }

    public void setPassword(String password) {
        this.password = password;
    }

    public String getRemarks() {
        return remarks;
    }

    public void setRemarks(String remarks) {
        this.remarks = remarks;
    }

    public String toString(){
        return "Server"         +   server          +"|"    +
                "Server_port"   +   server_port     +"|"    +
                "Local_port"    +   local_port      +"|"     +
                "Pwd"           +     password        +"|"      +
                "method"        +     method           +"|"     +
                "remarks"       +   remarks;
    }
}
