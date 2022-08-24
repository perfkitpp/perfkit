import React, {createContext, useEffect, useState} from 'react';
import logo from './logo.svg';
import './App.css';
import Terminal from "./comp/Terminal";

export const authContext = createContext(null as string | null);
export const socketUrlPrefix = process.env.NODE_ENV === "development" ? "ws://localhost:10021" : window.location.host

function App() {
  const [text, setText] = useState("-- empty --");

  useEffect(() => {
    fetch('api/test')
      .then(async (resp) => {
        console.log("Fetched!")
        setText(await resp.text());
      });
  }, []);

  return (
    <div className="App">
      <header className="App-header">
        <img src={logo} className="App-logo" alt="logo"/>
        <p>
          Edit <code>src/App.tsx</code> and save to reload.
        </p>
        <a
          className="App-link"
          href="https://reactjs.org"
          target="_blank"
          rel="noopener noreferrer"
        >
          Learn React
        </a>
        <Terminal socketUrl={socketUrlPrefix + '/ws/tty'}></Terminal>
        {text}
      </header>
    </div>
  );
}

export default App;
