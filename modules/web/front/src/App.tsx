import React, {useState} from 'react';
import logo from './logo.svg';
import './App.css';

function App() {
  const [text, setText] = useState("-- empty --");

  fetch('/api/test')
    .then(async (resp) => {
      console.log("Fetched!")
      setText(await resp.text());
    });

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
        {text}
      </header>
    </div>
  );
}

export default App;
