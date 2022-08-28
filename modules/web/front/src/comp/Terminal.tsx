import {useContext, useEffect, useRef, useState} from "react";
import 'react-bootstrap/Button';
import {Button, Container, Row, Spinner} from "react-bootstrap";
import {useForceUpdate, useWebSocket} from "../Utils";

export let AppendTextToTerminal: (payload: string, className?: string) => void
  = (payload, className) => {
};

export default function Terminal(props: { socketUrl: string }) {
  const [scrollLock, setScrollLock] = useState(false);
  const divRef = useRef(null as any as HTMLDivElement);
  const forceUpdate = useForceUpdate();

  const ttySock = useWebSocket(props.socketUrl, {
    onmessage: async ev => {
      appendText(await (ev as any).data.text());
    },
    onopen: ev => {
      forceUpdate();
    }
  });

  function appendText(payload: string, className: null | string = null) {
    let elem = divRef.current;
    if (elem == null) {
      return;
    }

    if (className == null) {
      if (elem.lastElementChild == null || !(elem.lastElementChild instanceof HTMLSpanElement)) {
        elem.innerHTML += `<span></span>`
      }

      (elem.lastElementChild as HTMLSpanElement).innerText += payload;
    } else {
      // if (elem.lastElementChild != null && (elem.lastElementChild instanceof HTMLSpanElement)) {
      // }
      payload = payload.replace('\n', '<br/>');
      let docstr = `<div class="">`;
      docstr +=
        `<div class="${className} p-1" title="${new Date().toLocaleString()}">` +
        `<span class="me-2 bg-secondary bg-opacity-100 px-2 text-white rounded-1">${new Date().toLocaleString()}</span>` +
        `${payload}` +
        `</div>`;
      docstr += `</div>`;
      elem.innerHTML += docstr;
    }

    if (!scrollLock) {
      elem.scrollTop = elem.scrollHeight;
    }
  }

  useEffect(() => {
    AppendTextToTerminal = appendText;
    return () => {
      AppendTextToTerminal = () => {
      };
    }
  }, []);

  async function onSubmitCommand(event: React.FormEvent<HTMLFormElement>) {
    ttySock?.send((event.target as any).command_content.value);
    appendText(`(cmd) ${(event.target as any).command_content.value}`, 'text-secondary');
    (event.target as any).command_content.value = "";
  }

  if (ttySock?.readyState != WebSocket.OPEN)
    return (<div className='text-center p-3 text-primary'><Spinner animation='border'></Spinner></div>);

  return (
    <div style={{display: "flex", height: '100%', flexDirection: 'column', fontFamily: 'Lucida Console, monospace'}}>
      <div className='p-2' style={{flex: '1', flexBasis: '200px', maxHeight: '50vh'}}>
        <p className='border border-secondary border-2 rounded-3 p-2 overflow-auto mb-0 bg-white h-100'
           ref={divRef}
           style={{}}></p>
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
          <input type='text' className='form-text w-100 mt-0 ps-2' name='command_content'
                 placeholder={'(enter command here)'}
                 style={{background: ttySock != null ? 'white' : '#c90000'}}/>
          <Button type='submit' className='ms-2 px-4' variant={'outline-primary'}>Send</Button>
        </form>
      </div>
    </div>);
}

