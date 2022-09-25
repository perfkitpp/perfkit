import {useReducer, useState} from "react";
import {useWebSocket} from "../Utils";
import {Spinner} from "react-bootstrap";

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
    } else {
      // Parse as jpeg binary ... create URL,
    }
  }

  const sockGraphic = useWebSocket(prop.socketUrl, {onmessage: onRecvMsg}, []);
  const [regenFence, regenerateList] = useReducer(s => s + 1, 0);

  if (sockGraphic?.readyState != WebSocket.OPEN)
    return (<div className='text-center p-3 text-primary'><Spinner animation='border'></Spinner></div>);

  return <div>CONNECTED!</div>
}

type GraphicContainer = Map<number, GraphicContext>;

interface GraphicContext {
  cachedJpegUrl: Blob,

}

interface MethodNewWindows {

}

interface MethodDeletedWindow {

}

function GraphicWindow(prop: {}) {
  return <div/>
}


