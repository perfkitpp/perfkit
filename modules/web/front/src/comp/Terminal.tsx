import {useContext, useEffect, useRef, useState} from "react";
import 'react-bootstrap/Button';
import {Button, Container, Row, Spinner} from "react-bootstrap";
import {EmptyFunc, useForceUpdate, useWebSocket} from "../Utils";
import {theme} from "../App";

export let AppendTextToTerminal: (payload: string, className?: string) => void
  = EmptyFunc;

export default function Terminal(props: { socketUrl: string }) {
  const scrollLock = useRef(false);
  const [textWrap, setTextWrap] = useState(false);
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
        `<span class="me-2 bg-secondary bg-opacity-50 px-2 text-light rounded-1">${new Date().toLocaleString()}</span>` +
        `${payload}` +
        `</div>`;
      docstr += `</div>`;
      elem.innerHTML += docstr;
    }

    if (!scrollLock.current) {
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
    appendText(`(cmd) ${(event.target as any).command_content.value}`, '');
    (event.target as any).command_content.value = "";
  }

  if (ttySock?.readyState != WebSocket.OPEN)
    return (<div className='text-center p-3 text-primary'><Spinner animation='border'></Spinner></div>);

  return (
    <div
      className='mx-2 mt-0 pb-2 px-1'
      style={{display: "flex", height: '100%', flexDirection: 'column', fontFamily: 'Lucida Console, monospace'}}>
      <p
        className={'border border-secondary text-white text-opacity-75 flex-grow-1 border-2 rounded-3 my-2 p-2 overflow-auto bg-black ' + (textWrap ? '' : 'text-nowrap')}
        ref={divRef}
        style={{color: theme.light, fontSize: '10pt'}}/>
      <div className='px-0 mb-1 flex-grow-0 rounded-3 px-2 py-2 bg-opacity-10 bg-secondary border border-secondary'>
        <div className='d-flex flex-row m-0'>
          <i
            className={'btn p-0 px-3 mx-1 '
              + (scrollLock.current
                ? 'ri-lock-fill btn-danger'
                : 'ri-lock-unlock-line')}
            title="Scroll Lock"
            onClick={ev => {
              scrollLock.current = !scrollLock.current;
              forceUpdate()
            }}/>
          <i className={'btn p-0 px-3 mx-1 '
            + (textWrap
              ? 'ri-text-wrap btn-success'
              : 'ri-text-wrap')}
             title="Text Wrap"
             onClick={() => setTextWrap(v => !v)}/>
          <span className='flex-grow-1'/>
          <div className='btn btn-danger px-5 py-0 ms-2 ri-delete-bin-5-line'
               onClick={() => divRef.current.innerHTML = ""}
               title='Clear output'
          />
        </div>
        <hr className='m-1'/>
        <div className='flex-grow-0'>
          <form className='d-flex pt-1 mb-1 flex-row' action='javascript:'
                onSubmit={ev => onSubmitCommand(ev)}>
            <input type='text' className='form-text w-100 mt-0 ms-2 me-1 ps-2 rounded-3' name='command_content'
                   placeholder={'(enter command here)'}
                   style={{background: ttySock != null ? theme.dark : theme.danger}}/>
            <Button type='submit' className='ri-send-plane-fill ms-2 px-4'
                    title='Send command'
                    variant={'outline-primary'}/>
          </form>
        </div>
      </div>
    </div>);
}

