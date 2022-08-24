import {useContext, useEffect, useRef, useState} from "react";
import {XTerm} from "xterm-for-react";
import {authContext} from "../App";

function Terminal(props: { socketUrl: string }) {
  const termRef = useRef(null as unknown as XTerm);
  const [ttySock, setTtySock] = useState(null as unknown as WebSocket);
  const auth = useContext(authContext);

  useEffect( ()=> {
    let sock = new WebSocket(props.socketUrl);
    

  }, [props.socketUrl]);

  useEffect(() => {
    const term = termRef.current.terminal;
    term.writeln("Hell, world!")
  });

  return <XTerm ref={termRef}></XTerm>;
}

export default Terminal;
