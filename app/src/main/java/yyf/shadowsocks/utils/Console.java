package yyf.shadowsocks.utils;

/**
 * Created by yyf on 2015/6/26.
 */
import java.util.List;

import eu.chainfire.libsuperuser.Shell;
import eu.chainfire.libsuperuser.Shell.*;

//import java.util

public class Console {


    static private Shell.Interactive openShell() {
        Builder builder = new Builder();
        return builder.useSH().setWatchdogTimeout(10).open();
    }

    static private Shell.Interactive openRootShell(String context) {
        Builder builder = new Builder();
        return builder.setShell(SU.shell(0, context)).setWantSTDERR(true).setWatchdogTimeout(10).open();
    }

    static public void runCommand(String command) {
        final Interactive shell = openShell();
        shell.addCommand(command, 0, new OnCommandResultListener() {
            @Override
            public void onCommandResult(int commandCode, int exitCode, List<String> output) {
                if (exitCode < 0) {
                    shell.close();
                }
            }
        });
        shell.waitForIdle();
        shell.close();
    }

    static public void runCommand(List<String> commands) {
        final Interactive shell = openShell();
        shell.addCommand(commands, 0, new OnCommandResultListener() {
            @Override
            public void onCommandResult(int commandCode, int exitCode, List<String> output) {
                if (exitCode < 0) {
                    shell.close();
                }
            }
        });
        shell.waitForIdle();
        shell.close();
    }

    static public String runRootCommand(String command) {
        return runRootCommand(command, "u:r:init_shell:s0");
    }

    static public String runRootCommand(String command, String context) {
        if (!isRoot()) {
            return null;
        }
        final Interactive shell = openRootShell(context);
        StringBuilder sb = new StringBuilder();
        shell.addCommand(command, 0, new OnCommandResultListener() {
            @Override
            public void onCommandResult(int commandCode, int exitCode, List<String> output) {
                if (exitCode < 0) {
                    shell.close();
                } else {
                    //阿勒阿列
                }
            }
        });
        if (shell.waitForIdle()) {
            shell.close();
            return sb.toString();
        } else {
            shell.close();
            return null;

        }
    }
    static public String runRootCommand(String[] commands) {
        return runRootCommand(commands, "u:r:init_shell:s0");
    }

    static public String runRootCommand(String[] commands, String context) {
        if (!isRoot()) {
            return null;
        }
        final Interactive shell = openRootShell(context);
        StringBuilder sb = new StringBuilder();
        shell.addCommand(commands, 0, new OnCommandResultListener() {
            @Override
            public void onCommandResult(int commandCode, int exitCode, List<String> output) {
                if (exitCode < 0) {
                    shell.close();
                } else {
                    //阿勒阿列
                }
            }
        });
        if (shell.waitForIdle()) {
            shell.close();
            return sb.toString();
        } else {
            shell.close();
            return null;
        }
    }

    static public boolean isRoot() {
        return SU.available();
    }
    static public String mkCMD(String[] cmd){
        String c = "";
        for(int i = 0;i<cmd.length;i++)
           c+=cmd[i]+" ";
        return c;
    }
}


