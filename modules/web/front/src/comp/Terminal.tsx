import {useContext, useEffect, useRef, useState} from "react";
import 'react-bootstrap/Button';
import {Button, Container, Row, Spinner} from "react-bootstrap";
import {useForceUpdate, useWebSocket} from "../Utils";

export let AppendTextToTerminal: (payload: string, className?: string) => void
  = (payload, className) => {
};

export default function Terminal(props: { socketUrl: string }) {
  const scrollLock = useRef(false);
  const divRef = useRef(null as any as HTMLDivElement);
  const forceUpdate = useForceUpdate();

  const ttySock = useWebSocket(props.socketUrl, {
    onmessage: async ev => {
      appendText(await (ev as any).data.text());
    },
    onopen: ev => {
      forceUpdate();
    }
  }, []);

  function appendText(payload: string, className: null | string = null) {
    const elem = divRef.current;
    if (elem == null) {
      return;
    }

    const srcScrollVal = elem.scrollTop;
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

    if (!scrollLock.current) {
      console.log('value chgange!!')
      elem.scrollTop = elem.scrollHeight;
    }
  }

  useEffect(() => {
    AppendTextToTerminal = appendText;
    return () => {
      AppendTextToTerminal = () => {
      };
    }
  }, [scrollLock.current]);

  async function onSubmitCommand(event: React.FormEvent<HTMLFormElement>) {
    ttySock?.send((event.target as any).command_content.value);
    appendText(`(cmd) ${(event.target as any).command_content.value}`, 'text-secondary');
    (event.target as any).command_content.value = "";
  }

  if (ttySock?.readyState != WebSocket.OPEN)
    return (<div className='text-center p-3 text-primary'><Spinner animation='border'></Spinner></div>);

  console.log('Redraw?')

  return (
    <div
      className='mx-2 mt-0 pb-2 px-1'
      style={{display: "flex", height: '100%', flexDirection: 'column', fontFamily: 'Lucida Console, monospace'}}>
      <p className='border border-secondary border-2 rounded-3 my-2 p-2 overflow-auto bg-white h-100'
         ref={divRef}
         style={{}}></p>
      <div className='px-0 mb-1 flex-grow-0 rounded-3 px-2 py-2 bg-opacity-10 bg-warning'>
        <div className='d-flex flex-row m-0'>
          <button className={'btn p-0 px-2 m-0 ' + (scrollLock.current ? 'btn-warning border-dark' : '')}
                  onClick={ev => {
                    scrollLock.current = !scrollLock.current;
                    forceUpdate()
                  }}>
            Scroll Lock
          </button>
          <span className='flex-grow-1'/>
          <div className='btn btn-danger px-2 py-0 ms-2'
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

