import {useContext, useEffect, useRef, useState} from "react";
import {authContext} from "../App";
import 'react-bootstrap/Button';
import 'bootstrap/dist/js/bootstrap.min'
import './Terminal.scss'
import {Button} from "react-bootstrap";


export default function Terminal(props: { socketUrl: string }) {
  const [ttySock, setTtySock] = useState(null as any as WebSocket);
  const [scrollLock, setScrollLock] = useState(false);
  const divRef = useRef(null as any as HTMLDivElement);
  const auth = useContext(authContext);

  function appendText(payload: string, className: null | string = null) {
    let elem = divRef.current;

    if (className == null) {
      if (!(elem.lastElementChild instanceof HTMLSpanElement)) {
        elem.innerHTML += `<span></span>`
      }

      (elem.lastElementChild as HTMLSpanElement).innerText += payload;
    } else {
      if ((elem.lastElementChild instanceof HTMLSpanElement)) {
        elem.innerHTML += `<br/>`
      }
      payload = payload.replace('\n', '<br/>');
      let docstr = `<div class="">`;
      docstr +=
        `<div class="${className} p-1" title="${new Date().toLocaleString()}">` +
        `<span class="me-2 bg-secondary bg-opacity-100 px-2 py-1 text-white rounded-1">${new Date().toLocaleString()}</span>` +
        `${payload}` +
        `</div>`;
      docstr += `</div>`;
      elem.innerHTML += docstr;
    }

    if (!scrollLock) {
      elem.scrollTop = elem.scrollHeight;
    }
  }

  async function onSubmitCommand(event: React.FormEvent<HTMLFormElement>) {
    ttySock.send((event.target as any).command_content.value);
    appendText(`(cmd) ${(event.target as any).command_content.value}`, 'text-secondary');
    (event.target as any).command_content.value = "";
  }

  useEffect(() => {
    if(!(ttySock == null || ttySock.CLOSED)) { return;}

    let sock = new WebSocket(props.socketUrl);
    setTtySock(sock);

    let str = ""
    for (let i = 0; i < 1000; ++i) {
      str += `[${i}] hello, world!\n`;
    }
    appendText(str);

    sock.onopen = () => {
      appendText('-- Socket Connected!', 'bg-success bg-opacity-25')
    };
    sock.onerror = ev => {
      appendText(`! Socket error: ${JSON.stringify(ev)}`);
    };
    sock.onmessage = ev => {
      appendText(ev.data);
    }
    sock.onclose = ev => {
      appendText(`-- WebSocket disconnected.`, `bg-warning bg-opacity-50`);
    }

    return () => {
      appendText('Closing socket ...', 'bg-warning bg-opacity-25')
      sock.close();
    };
  }, [props.socketUrl]);

  return <div className='terminal-output-box'>
    <div className='ri-terminal-line fw-bold w-100 pb-2 text-center' style={{fontSize: '1.2em'}}> Terminal</div>
    <p className='border border-secondary border-2 rounded-3 p-1 overflow-auto mb-1'
       ref={divRef}
       style={{maxHeight: '40vh', minHeight: '10vh', background: ttySock?.OPEN ? 'white' : 'dimgray'}}></p>
    <div className='d-flex m-2'>
      <input className='' type={"checkbox"} name={'scroll_lock'}
             onClick={ev => setScrollLock((ev.target as any).checked)}/>
      <label className='pt-1 ms-2' htmlFor='scroll_lock'>Scroll Lock</label>

      <div className='btn btn-outline-danger px-2 py-0 ms-2'
           onClick={() => divRef.current.innerHTML = ""}>
        clear contents
      </div>
    </div>
    <form className='d-flex pt-1 mb-1' action='javascript:'
          onSubmit={ev => onSubmitCommand(ev)}>
      <span className='input-group-text me-1'>@</span>
      <input type='text' className='form-text w-100 mt-0' name='command_content' placeholder={'-- command'}/>
      <Button type='submit' className='ms-2 px-4' variant={'outline-primary'}>Send</Button>
    </form>
  </div>;
}

