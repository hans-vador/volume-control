import { useEffect, useMemo, useRef, useState } from "react";
import "./App.css";

const DEFAULT_BASE = "http://192.168.1.12";

export default function App() {
  const [base, setBase] = useState(DEFAULT_BASE);
  const [status, setStatus] = useState("idle");
  const [value, setValue] = useState(0); // 0..100 slider position
  const [isConnected, setIsConnected] = useState(false);
  const pending = useRef(null);

  async function sendPing() {
    setStatus("contacting board…");
    try {
      const res = await fetch(`${base}/ping`, { method: "GET" });
      const text = await res.text();
      setStatus(`${res.status}: ${text}`);
      setIsConnected(true);
    } catch (e) {
      setStatus(`ERROR: ${e.message}`);
      setIsConnected(false);
    }
  }

  // Debounced volume updates so we don't spam the board while sliding
  const sendVolume = useMemo(
    () => {
      let timeout;
      return (level) => {
        if (timeout) clearTimeout(timeout);
        timeout = setTimeout(async () => {
          setStatus(`setting volume to ${level}%…`);
          // cancel any in-flight request
          if (pending.current) {
            try { pending.current.abort(); } catch { }
          }
          const ctrl = new AbortController();
          pending.current = ctrl;
          try {
            const res = await fetch(`${base}/volume?level=${level}`, {
              method: "GET",
              signal: ctrl.signal,
            });
            const text = await res.text();
            setStatus(`${res.status}: ${text}`);
          } catch (e) {
            if (e.name !== "AbortError") setStatus(`ERROR: ${e.message}`);
          } finally {
            pending.current = null;
          }
        }, 100); // ~1 update per 180ms while sliding
      };
    },
    [base]
  );

  useEffect(() => {
    // on mount, quickly check connectivity
    sendPing();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  return (
    <div className="app">
      <div className="container">
        <header className="header">
          <div className="logo">
            <div className="logo-icon">🎛️</div>
            <h1>Volume Control</h1>
          </div>
          <p className="subtitle">UNO R4 WiFi Remote Control</p>
        </header>

        <div className="connection-section">
          <div className="connection-header">
            <h2>Connection</h2>
            <div className={`status-indicator ${isConnected ? 'connected' : 'disconnected'}`}>
              <div className="status-dot"></div>
              <span>{isConnected ? 'Connected' : 'Disconnected'}</span>
            </div>
          </div>

          <div className="input-group">
            <label htmlFor="board-url">Board URL (HTTP):</label>
            <div className="input-with-button">
              <input
                id="board-url"
                type="text"
                value={base}
                onChange={(e) => setBase(e.target.value)}
                placeholder="http://192.168.1.42"
                spellCheck={false}
                className="url-input"
              />
              <button
                onClick={sendPing}
                className="ping-button"
                disabled={!base.trim()}
              >
                <span className="button-icon">🔗</span>
                Test Connection
              </button>
            </div>
          </div>
        </div>

        <div className="volume-section">
          <div className="volume-header">
            <h2>Volume Control</h2>
            <div className="volume-display">
              <span className="volume-value">{value}</span>
              <span className="volume-unit">%</span>
            </div>
          </div>

          <div className="slider-container">
            <input
              type="range"
              min={0}
              max={100}
              step={1}
              value={value}
              onChange={(e) => {
                const v = Number(e.target.value);
                setValue(v);
                sendVolume(v);
              }}
              className="volume-slider"
              disabled={!isConnected}
            />
            <div className="slider-labels">
              <span>0%</span>
              <span>100%</span>
            </div>
          </div>
        </div>

        <div className="status-section">
          <h3>Status</h3>
          <div className="status-message">
            {status}
          </div>
        </div>
      </div>
    </div>
  );
}
