package com.mercrecomp;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.StatFs;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;
import java.io.FileWriter;

/**
 * Picks the user's own original-Xbox disc image (.iso) through the Storage
 * Access Framework and extracts it straight from the file descriptor (no
 * intermediate copy) into app-private storage, then starts the game.
 */
public class LauncherActivity extends Activity {
    private boolean libOk;
    private String loadErr = "";
    private static final int REQ_ISO = 1;

    private native String nativeExtract(int fd, String destDir);
    private native void nativeCancel();
    private native boolean nativeHasGame();

    /** Older native libs do not have nativeHasGame(); treat that as "game present". */
    private boolean hasGame() {
        try { return nativeHasGame(); } catch (UnsatisfiedLinkError e) { return true; }
    }

    private TextView status;
    private ProgressBar bar;
    private Button pick, play, share;

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(48, 48, 48, 48);
        status = new TextView(this);
        bar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        bar.setMax(1000);
        pick = new Button(this); pick.setText("Import Xbox disc image (.iso)");
        play = new Button(this); play.setText("Play");
        share = new Button(this); share.setText("Share last game log");
        root.addView(status); root.addView(bar); root.addView(pick); root.addView(play); root.addView(share);
        setContentView(root);
        pick.setOnClickListener(v -> {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("*/*");
            startActivityForResult(i, REQ_ISO);
        });
        installCrashCapture();
        try { System.loadLibrary("mercrecomp"); libOk = true; } catch (Throwable t) { loadErr = t.toString(); }
        play.setOnClickListener(v -> startGame());
        share.setOnClickListener(v -> shareLog());
        refresh();
    }

    private File gameDir() { return new File(getFilesDir(), "game"); }
    private boolean imported() { return new File(gameDir(), "default.xbe").isFile(); }

    private void installCrashCapture() {
        final Thread.UncaughtExceptionHandler prev = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((t, e) -> {
            try (FileWriter w = new FileWriter(new File(getFilesDir(), "java_crash.txt"))) {
                w.write(android.util.Log.getStackTraceString(e));
            } catch (Throwable ignored) { }
            if (prev != null) prev.uncaughtException(t, e);
        });
    }

    private void refresh() {
        File jc = new File(getFilesDir(), "java_crash.txt");
        if (jc.isFile()) {
            status.setText("Previous Java crash (tap Share last game log to send it):\n" + tail(jc, 1500));
        }
        if (!libOk) {
            pick.setEnabled(false); play.setEnabled(false);
            status.setText("Native library failed to load:\n" + loadErr + "\nTap Share last game log and send it.");
            return;
        }
        if (jc.isFile()) { play.setEnabled(imported()); return; }
        if (!hasGame()) {
            play.setEnabled(false);
            status.setText("This APK was built WITHOUT the game code (recompiler output missing), so it cannot run the game. "
                + "Run tools/recomp on your own disc on a PC, then rebuild with -PmercGenDir=<gen folder>.");
            return;
        }
        play.setEnabled(imported());
        status.setText(imported()
            ? "Game data imported. Free space: " + (new StatFs(getFilesDir().getPath()).getAvailableBytes() >> 20) + " MB"
            : "Import your own original Xbox disc image of Mercenaries. Nothing is downloaded or bundled.");
    }

    @Override protected void onActivityResult(int req, int res, Intent data) {
        if (req != REQ_ISO || res != RESULT_OK || data == null) return;
        final Uri uri = data.getData();
        pick.setEnabled(false); play.setEnabled(false);
        new Thread(() -> {
            String err;
            try (ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, "r")) {
                err = nativeExtract(pfd.detachFd(), gameDir().getAbsolutePath());
            } catch (Exception e) { err = String.valueOf(e.getMessage()); }
            final String msg = err;
            runOnUiThread(() -> { pick.setEnabled(true); if (!msg.isEmpty()) status.setText("Import failed: " + msg); refresh(); });
        }).start();
    }

    /** Called from native code. */
    public void onProgress(long done, long total, String path) {
        runOnUiThread(() -> {
            bar.setProgress(total > 0 ? (int) (done * 1000 / total) : 0);
            status.setText("Importing… " + (done >> 20) + " / " + (total >> 20) + " MB\n" + path);
        });
    }

    private String tail(File f, int max) {
        try (java.io.RandomAccessFile r = new java.io.RandomAccessFile(f, "r")) {
            long len = r.length(), keep = Math.min(len, max);
            byte[] b = new byte[(int) keep]; r.seek(len - keep); r.readFully(b);
            return new String(b, "UTF-8");
        } catch (Exception e) { return ""; }
    }

    private void shareLog() {
        String crash = tail(new File(getCacheDir(), "crash.log"), 8000);
        String log = tail(new File(getCacheDir(), "game.log"), 50000);
        if (crash.isEmpty() && log.isEmpty() && tail(new File(getCacheDir(), "events.log"), 10).isEmpty() && libOk && !new File(getFilesDir(), "java_crash.txt").isFile()) { status.setText("No game log yet. Press Play first."); return; }
        String jcrash = tail(new File(getFilesDir(), "java_crash.txt"), 6000);
        if (!libOk) jcrash = "LOAD ERROR: " + loadErr + "\n" + jcrash;
        String events = tail(new File(getCacheDir(), "events.log"), 30000);
        String text = "=== events.log (end) ===\n" + events + "\n=== java crash ===\n" + jcrash + "\n=== crash.log ===\n" + crash + "\n=== game.log (end) ===\n" + log;
        Intent i = new Intent(Intent.ACTION_SEND);
        i.setType("text/plain");
        i.putExtra(Intent.EXTRA_TEXT, text);
        startActivity(Intent.createChooser(i, "Share game log"));
    }

    private void startGame() {
        new File(getCacheDir(), "crash.log").delete();
        new File(getFilesDir(), "java_crash.txt").delete();
        try (FileWriter w = new FileWriter(new File(getFilesDir(), "launch.cfg"))) {
            w.write("game=" + gameDir().getAbsolutePath() + "\n");
            w.write("save=" + new File(getFilesDir(), "saves").getAbsolutePath() + "\n");
            w.write("tmp=" + getCacheDir().getAbsolutePath() + "\n");
        } catch (Exception e) { status.setText("Cannot write launch config: " + e.getMessage()); return; }
        startActivity(new Intent(this, GameActivity.class));
    }
}
