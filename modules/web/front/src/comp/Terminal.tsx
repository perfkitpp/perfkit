import {useContext, useEffect, useRef, useState} from "react";
import {authContext} from "../App";
import 'react-bootstrap/Button';
import './Terminal.css'

export default function Terminal(props: { socketUrl: string }) {
  const [ttySock, setTtySock] = useState(null as any as WebSocket);
  const divRef = useRef(null as any as HTMLDivElement);
  const auth = useContext(authContext);

  useEffect(() => {
    let sock = new WebSocket(props.socketUrl);
    setTtySock(sock);

    sock.onopen = () => {
      console.log(`-- Socket successfully connected`);
      divRef.current.innerText += `-- Socket successfully connected\n`;
    };
    sock.onerror = ev => {
      console.log(`! Socket error: ${JSON.stringify(ev)}`);
      divRef.current.innerText += `-- Socket error: ${JSON.stringify(ev)}\n`;
    };
    sock.onmessage = ev => {
      console.log(`-- Socket data: ${JSON.stringify(ev.data)}`);
      divRef.current.innerText += ev.data;
    }

    return () => {
      console.log("-- Closing socket ...")
      sock.close();
    };
  }, [props.socketUrl]);

  return <div>
    <div className='terminal-output-box' ref={divRef}></div>
    <div className='terminal-input-box'></div>
  </div>;
}

