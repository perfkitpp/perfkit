import {useContext, useEffect, useRef, useState} from "react";
import 'react-bootstrap/Button';
import 'bootstrap/dist/js/bootstrap.min'
import './Terminal.scss'
import {Button, Container, Row} from "react-bootstrap";


export default function Terminal(props: { socketUrl: string }) {
  const ttySockRef = useRef(null as any as WebSocket);
  const [refreshSock, setRefreshSock] = useState(true);
  const [scrollLock, setScrollLock] = useState(false);
  const [isConnected, setIsConnected] = useState(false);
  const divRef = useRef(null as any as HTMLDivElement);

  function appendText(payload: string, className: null | string = null) {
    let elem = divRef.current;

    if (className == null) {
      if (elem.lastElementChild == null || !(elem.lastElementChild instanceof HTMLSpanElement)) {
        elem.innerHTML += `<span></span>`
      }

      (elem.lastElementChild as HTMLSpanElement).innerText += payload;
    } else {
      if (elem.lastElementChild != null && (elem.lastElementChild instanceof HTMLSpanElement)) {
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
    ttySockRef.current?.send((event.target as any).command_content.value);
    appendText(`(cmd) ${(event.target as any).command_content.value}`, 'text-secondary');
    (event.target as any).command_content.value = "";
  }

  useEffect(() => {
    if (!refreshSock) {
      return;
    }
    if (ttySockRef.current != null) {
      return;
    }
    setRefreshSock(false);

    let sock = new WebSocket(props.socketUrl);
    ttySockRef.current = sock;

    appendText('-- Start connecting to socket ...', 'bg-success bg-opacity-25')

    sock.onopen = () => {
      appendText('-- Socket Connected!', 'bg-success bg-opacity-25')
      setIsConnected(true);
    };
    sock.onerror = ev => {
      appendText(`! Socket error: ${JSON.stringify(ev)}`, `bg-danger bg-opacity-50`);
    };
    sock.onmessage = ev => {
      appendText(ev.data);
    }
    sock.onclose = ev => {
      setIsConnected(false);
      appendText(`-- Socket disocnnected.`, `bg-info bg-opacity-50`)
      appendText(`-- Will try reconnect in 5 seconds ...`, `bg-info bg-opacity-50`);
      setTimeout(() => {
        ttySockRef.current = null as any as WebSocket;
        setRefreshSock(true)
      }, 5000);
    }
  }, [props.socketUrl, refreshSock]);

  return (
    <div style={{display: "flex", height: '100%', flexDirection: 'column'}}>
      <div className='p-2' style={{flex: '1', flexBasis: '200px'}}>
        <p className='border border-secondary border-2 rounded-3 p-2 overflow-auto mb-0 bg-white h-100'
           ref={divRef}
           style={{height: ''}}></p>
      </div>
      <div className='px-0 mb-2 flex-grow-0'>
        <div className='d-flex flex-row m-0'>
          <input className='' type={"checkbox"} name={'scroll_lock'}
                 onClick={ev => setScrollLock((ev.target as any).checked)}/>
          <label className='pt-1 ms-2' htmlFor='scroll_lock'>Scroll Lock</label>
          <div className='btn btn-outline-danger px-2 py-0 ms-2'
               onClick={() => divRef.current.innerHTML = ""}>
            clear contents
          </div>
        </div>
      </div>
      <div className=''>
        <form className='d-flex pt-1 mb-1 flex-row' action='javascript:'
              onSubmit={ev => onSubmitCommand(ev)}>
          <span className='input-group-text me-1'>@</span>
          <input type='text' className='form-text w-100 mt-0' name='command_content' placeholder={'-- command'}
                 style={{background: isConnected ? 'white' : '#c90000'}}/>
          <Button type='submit' className='ms-2 px-4' variant={'outline-primary'}>Send</Button>
        </form>
      </div>
    </div>);
}

