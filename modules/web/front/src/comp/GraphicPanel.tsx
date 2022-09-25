import {startTransition, useEffect, useMemo, useReducer, useRef, useState} from "react";
import {useForceUpdate, useWebSocket} from "../Utils";
import {Spinner} from "react-bootstrap";
import {CloseWindow, CreateWindow} from "../App";
import {Simulate} from "react-dom/test-utils";

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
  const canvasParentDivRef = useRef({} as HTMLDivElement);
  const canvasRef = useRef({} as HTMLCanvasElement);
  const forceUpdate = useForceUpdate();
  const toggleFwdMode = () => (stateRef.current.fwdMode = !stateRef.current.fwdMode, forceUpdate());
  const stateRef = useRef({
    cachedImage: new Image(),
    isFirstLoad: true,
    backBuffer: {} as HTMLCanvasElement,
    imgOffst: {x: 0, y: 0},
    zoom: 1,
    fwdMode: false,
  });

  useEffect(() => {
    stateRef.current.backBuffer = document.createElement('canvas');
    context.onUpdateJpeg =
      function onUpdateJpeg() {
        if (!context.cachedJpeg) return;

        const img = new Image();
        const state = stateRef.current;
        img.src = URL.createObjectURL(context.cachedJpeg);
        img.onload = () => {
          state.cachedImage = img;
          if (state.isFirstLoad) {
            state.isFirstLoad = false;
            fitViewToImage();
          }
          invalidateSrc();
        }
      };

    canvasRef.current.width = canvasParentDivRef.current.clientWidth;
    canvasRef.current.height = canvasParentDivRef.current.clientHeight;

    new ResizeObserver((element, ev) => {
      const box = element.at(0)?.contentRect;
      if (!box) return;

      startTransition(() => {
        canvasRef.current.width = box.width;
        canvasRef.current.height = box.height - 10;
        stateRef.current.backBuffer.width = canvasRef.current.width;
        stateRef.current.backBuffer.height = canvasRef.current.height;
        invalidateDisp();
      });
    }).observe(canvasParentDivRef.current)

    canvasRef.current.onmousemove =
      function (ev) {
        if (stateRef.current.fwdMode) {
          // TODO: Forward event to server ..
        } else {
          if (ev.buttons & 1) {
            const state = stateRef.current;
            state.imgOffst.x += ev.movementX;
            state.imgOffst.y += ev.movementY;

            startTransition(invalidateDisp);
          }
        }
      };

    canvasRef.current.ondblclick =
      function (ev) {
        if (stateRef.current.fwdMode) {
          // TODO: Forward event to server ..
        } else {
          // Fit content window
          fitViewToImage();
        }
      }

    canvasRef.current.onwheel =
      function (ev) {
        if (stateRef.current.fwdMode) {
          // TODO: Forward event to server ..
        } else {
          stateRef.current.zoom = Math.pow(stateRef.current.zoom * 100, 1 - ev.deltaY * 1e-4) / 100;
          invalidateSrc();
        }
      };
  }, []);

  function invalidateSrc() {
    // TODO: Change backBuffer to use source image transform, and canvas to use offset context.
    invalidateDisp();
  }

  function fitViewToImage() {
    // Find smaller scale
    const {width, height} = canvasRef.current;
    const {naturalWidth, naturalHeight} = stateRef.current.cachedImage;
    const [scaleX, scaleY] = [width / naturalWidth, height / naturalHeight];
    const zoom = stateRef.current.zoom = Math.min(scaleX, scaleY);
    const ofst = stateRef.current.imgOffst;
    ofst.x = naturalWidth / 2 * zoom + (width - naturalWidth * zoom) / 2;
    ofst.y = naturalHeight / 2 * zoom + (height - naturalHeight * zoom) / 2;
    invalidateDisp();
  }

  function invalidateDisp() {
    const state = stateRef.current;
    const img = state.cachedImage;
    const canvas = state.backBuffer;
    const ctx = canvas.getContext('2d');
    const {x, y} = state.imgOffst;
    const {zoom} = state;

    if (!ctx) return;

    ctx.fillStyle = "dimgray";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    ctx.drawImage(img,
      (state.imgOffst.x - img.naturalWidth / 2 * zoom),
      (state.imgOffst.y - img.naturalHeight / 2 * zoom),
      img.naturalWidth * zoom,
      img.naturalHeight * zoom
    );

    canvasRef.current.getContext('2d')?.drawImage(canvas, 0, 0);
  }

  return <div className='d-flex flex-column h-100'>
    <div className='bg-dark px-2 d-flex flex-row gap-1'>
      <i
        className={'btn py-0 px-3 ' + (stateRef.current.fwdMode ? 'ri-edit-circle-fill btn-danger ' : 'ri-focus-line ')}
        onClick={toggleFwdMode}
      />
      <i className='btn py-0 px-2 ri-drag-move-2-fill' onClick={fitViewToImage}/>
    </div>
    <div className='flex-grow-1' ref={canvasParentDivRef}>
      <canvas
        ref={canvasRef}
      />
    </div>
  </div>
}


