import {useContext, useEffect, useRef, useState} from "react";
import {XTerm} from "xterm-for-react";
import {authContext} from "../App";
import {useForceUpdate, useInterval} from "../Utils"

function Terminal(props: { socketUrl: string }) {
  const termRef = useRef(null as unknown as XTerm);
  const forceUpdate = useForceUpdate();
  const [ttySock, setTtySock] = useState(null as unknown as WebSocket);
  const auth = useContext(authContext);

  useEffect(() => {
    let sock = new WebSocket(props.socketUrl);
    setTtySock(sock);

    sock.onopen = () => {
      console.log(`-- Socket successfully connected`);

      forceUpdate();
    };
    sock.onerror = ev => {
      console.log(`! Socket error: ${JSON.stringify(ev)}`);
    };
    sock.onmessage = ev => {
      console.log(`-- Socket data: ${JSON.stringify(ev.data)}`);
    }

    return () => {
      console.log("-- Closing socket ...")
      sock.close();
    };
  }, [props.socketUrl]);

  useInterval(() => {
    forceUpdate();
  }, 1000);

  useEffect(()=>{
    const term = termRef.current.terminal;
    term.write("adsgfdas\n")
    term.write("adsgfdaskkkg\n")
    term.write("adsgfdaskkkg\n")
    term.write("adsgfdaskkkggfasdg\n")
  });

  return <XTerm ref={termRef}></XTerm>;
}

export default Terminal;
