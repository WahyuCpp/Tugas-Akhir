import React, { useState } from "react";
import {
  CognitoUserPool,
  CognitoUser,
  AuthenticationDetails,
} from "amazon-cognito-identity-js";
import { AWS_CONFIG } from "../aws-config";

const userPool = new CognitoUserPool({
  UserPoolId: AWS_CONFIG.userPoolId,
  ClientId: AWS_CONFIG.userPoolClientId,
});

export default function Login({ onAuthenticated }) {
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState(null);
  const [busy, setBusy] = useState(false);

  const handleSubmit = (e) => {
    e.preventDefault();
    setError(null);
    setBusy(true);

    const user = new CognitoUser({ Username: email, Pool: userPool });
    const authDetails = new AuthenticationDetails({
      Username: email,
      Password: password,
    });

    user.authenticateUser(authDetails, {
      onSuccess: (session) => {
        setBusy(false);
        onAuthenticated({
          idToken: session.getIdToken().getJwtToken(),
          accessToken: session.getAccessToken().getJwtToken(),
        });
      },
      onFailure: (err) => {
        setBusy(false);
        setError(err.message || "Authentication failed");
      },
      newPasswordRequired: () => {
        setBusy(false);
        setError("Account requires a new password — reset via Cognito console");
      },
    });
  };

  return (
    <div style={styles.container}>
      <form onSubmit={handleSubmit} style={styles.form}>
        <h2 style={styles.title}>Motor Monitor — Sign In</h2>
        <input
          type="email"
          placeholder="Email"
          value={email}
          onChange={(e) => setEmail(e.target.value)}
          style={styles.input}
          required
        />
        <input
          type="password"
          placeholder="Password"
          value={password}
          onChange={(e) => setPassword(e.target.value)}
          style={styles.input}
          required
        />
        {error && <div style={styles.error}>{error}</div>}
        <button type="submit" disabled={busy} style={styles.button}>
          {busy ? "Signing in..." : "Sign In"}
        </button>
      </form>
    </div>
  );
}

const styles = {
  container: {
    display: "flex",
    justifyContent: "center",
    alignItems: "center",
    height: "100vh",
    background: "#0f172a",
    fontFamily: "system-ui, sans-serif",
  },
  form: {
    display: "flex",
    flexDirection: "column",
    gap: "12px",
    width: "320px",
    padding: "32px",
    background: "#1e293b",
    borderRadius: "8px",
  },
  title: { color: "#e2e8f0", marginBottom: "8px", fontSize: "18px" },
  input: {
    padding: "10px",
    borderRadius: "4px",
    border: "1px solid #334155",
    background: "#0f172a",
    color: "#e2e8f0",
  },
  button: {
    padding: "10px",
    borderRadius: "4px",
    border: "none",
    background: "#2563eb",
    color: "white",
    cursor: "pointer",
  },
  error: { color: "#f87171", fontSize: "13px" },
};
