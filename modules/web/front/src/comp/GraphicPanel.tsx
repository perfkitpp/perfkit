import {createContext, useEffect, useMemo, useReducer, useRef, useState} from "react";
import {useForceUpdate, useWebSocket} from "../Utils";
import {Spinner} from "react-bootstrap";
import {CloseWindow, CreateWindow} from "../App";

/*
  About control flow ...
  1. Receives 'new-window'
  2. Displays list of labels on graphics list
  3.
 */

export default function GraphicPanel(prop: { socketUrl: string }) {
  function onRecvMsg(ev: MessageEvent) {
    if (typeof ev.data === "string") {
      // Parse as string command
      const msg = JSON.parse(ev.data) as MethodNewWindows | MethodDeletedWindow;
      switch (msg.method) {
        case "new_windows": {
          for (const [key, title] of msg.params) {
            cleanupWnd(key);
            allWnds.current.set(key, {id: key, title, watching: false, refSocket: socket as WebSocket});
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
      const blob = ev.data as Blob;
      const header = blob.slice(0, 64);
      const content = blob.slice(64, blob.size, 'image/jpeg');

      header.arrayBuffer().then(
        value => {
          const idBigI = new DataView(value).getBigUint64(0, true);
          const context = allWnds.current.get(Number(idBigI));
          if (context) {
            context.cachedJpeg = content;
            context.onUpdateJpeg && context.onUpdateJpeg();
          }
        }
      )
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
      forceUpdate();
    }

    useEffect(() => {
      context.onCloseWindow = () => {
        context.watching = false;
        forceUpdate();
      }
    }, []);

    useEffect(() => {
      socket?.send(JSON.stringify({
        method: 'wnd_control',
        params: {
          id: context.id,
          watching: context.watching
        }
      }));

      if (context.watching) {
        // Send watch start message, spawn window
        CreateWindow(MakeWndKey(context.id), {
          content: <GraphicWindow context={context}/>,
          onClose: context.onCloseWindow,
          title: `(gp ${context.id}) ${context.title}`,
          closable: true
        });
      } else {
        // Send watch stop message, close window
        CloseWindow(MakeWndKey(context.id));
      }
    }, [context.watching]);

    return <div className='d-flex flex-row w-100'>
      <div
        className={'btn text-start mb-1 mx-1 py-0 flex-grow-1 ' + (context.watching && 'btn-success ')}
        onClick={onClick}>
        <span className='text-secondary me-2'>({context.id})</span>
        {context.title}
      </div>
    </div>
  }

  const socket = useWebSocket(prop.socketUrl, {onmessage: onRecvMsg}, []);
  const allWnds = useRef(new Map<number, GraphicContext>);
  const [fenceListRegen, notifyListDirty] = useReducer(s => s + 1, 0);

  useEffect(() => {
    return () => {
      // Close all opened windows
      for (const key of Array.from(allWnds.current.keys())) {
        cleanupWnd(key);
      }
    };
  }, []);

  const labels = useMemo(() =>
      Array.from(allWnds.current)
        .sort(([, a], [, b]) => a.title < b.title ? -1 : 0)
        .map(([key, context]) => <GraphicNodeLabel key={key} context={context}/>),
    [fenceListRegen]);

  if (socket?.readyState != WebSocket.OPEN)
    return (<div className='text-center p-3 text-primary'><Spinner animation='border'></Spinner></div>);

  return <div className='pt-2'>
    <div className='mx-1' style={{overflowY: 'auto', fontFamily: 'Lucida Console, monospace'}}>
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
  onUpdateJpeg?: () => void;

  refSocket: WebSocket
}

interface MethodNewWindows {
  method: 'new_windows',
  params: [number, string][]
}

interface MethodDeletedWindow {
  method: 'deleted_window',
  params: number
}

function GraphicWindow(prop: { context: GraphicContext }) {
  const {context} = prop;
  const imgRef = useRef({} as HTMLImageElement);

  useEffect(() => {
    context.onUpdateJpeg = () => {
      if (!context.cachedJpeg) return;

      const url = URL.createObjectURL(context.cachedJpeg);
      imgRef.current.src = url;

      console.log(url, context.cachedJpeg);
    };
  }, []);

  console.log("PewPEw");
  return <div>
    <img ref={imgRef} alt={context.title}/>
  </div>
}


