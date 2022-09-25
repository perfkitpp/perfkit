import {useMemo, useReducer, useRef, useState} from "react";
import {useForceUpdate, useWebSocket} from "../Utils";
import {Spinner} from "react-bootstrap";
import {CloseWindow} from "../App";

/*
  About control flow ...
  1. Receives 'new-window'
  2. Displays list of labels on graphics list
  3.
 */
export default function GraphicPanel(prop: { socketUrl: string }) {
  function onRecvMsg(ev: MessageEvent) {
    console.log(ev.data);
    if (typeof ev.data === "string") {
      // Parse as string command
      const msg = JSON.parse(ev.data) as MethodNewWindows | MethodDeletedWindow;
      switch (msg.method) {
        case "new_windows": {
          for (const [key, title] of msg.params) {
            cleanupWnd(key);
            allWnds.current.set(key, {id: key, title, watching: false});
          }

          notifyListDirty();
          break;
        }

        case "deleted_window": {
          cleanupWnd(msg.params);
          notifyListDirty();
          break;
        }
      }
    } else {
      // Parse as jpeg binary ... create URL,
      console.log(ev.data);
    }
  }

  function cleanupWnd(key: number) {
    if (allWnds.current.delete(key)) {
      CloseWindow(MakeWndKey(key));
    }
  }

  function GraphicNodeLabel(prop: { context: GraphicContext }) {
    const {context} = prop;
    const forceUpdate = useForceUpdate();

    function onClick() {
      context.watching = !context.watching;
      if (context.watching) {
        // TODO: Send watch start message, spawn window
      } else {
        // TODO: Send watch stop message, close window

      }
      forceUpdate();
    }

    return <div className='d-flex flex-row w-100'>
      <div
        className={'btn text-start mb-1 mx-1 py-0 flex-grow-1 ' + (context.watching && 'btn-primary')}
        onClick={onClick}>
        {context.title}
      </div>
    </div>
  }

  const sockGraphic = useWebSocket(prop.socketUrl, {onmessage: onRecvMsg}, []);
  const allWnds = useRef(new Map<number, GraphicContext>);
  const [fenceListRegen, notifyListDirty] = useReducer(s => s + 1, 0);

  const labels = useMemo(() =>
      Array.from(allWnds.current)
        .sort(([, a], [, b]) => a.title < b.title ? -1 : 0)
        .map(([key, context]) => <GraphicNodeLabel key={key} context={context}/>),
    [fenceListRegen]);

  if (sockGraphic?.readyState != WebSocket.OPEN)
    return (<div className='text-center p-3 text-primary'><Spinner animation='border'></Spinner></div>);

  return <div className='pt-2'>
    <div className='mx-1' style={{overflowY: 'auto'}}>
      {labels}
    </div>
  </div>
}

function MakeWndKey(key: number) {
  return `graphic/wnd/${key}`
}

interface GraphicContext {
  id: number,
  title: string,
  cachedJpeg?: Blob,

  watching: boolean,
  onCloseWindow?: () => void;

}

interface MethodNewWindows {
  method: 'new_windows',
  params: [number, string][]
}

interface MethodDeletedWindow {
  method: 'deleted_window',
  params: number
}

function GraphicWindow(prop: {}) {
  return <div/>
}


