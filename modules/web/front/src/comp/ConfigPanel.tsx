import {createContext, useEffect, useRef, useState} from "react";
import {useForceUpdate, useWebSocket} from "../Utils";
import {Spinner} from "react-bootstrap";

interface ElemContext {
  updateFence: number;
  description: string;
  name: string;
  minValue?: any;
  maxValue?: any;
  children: string[];

  value: any;
}

interface ElemContextTable {
  [key: string]: ElemContext
}

function ElemNode(props: {
  configID: string;
  contextTable: ElemContextTable;
}) {
}

function EditorPopup(props: {
  configID: string;
  targetContext: ElemContext;
}) {

}

interface ReceivedMessage {
  method: "update" | "new-cfg" | "delete-cfg",
  params: [any]
}

export default function ConfigPanel(props: { socketUrl: string }) {
  // TODO: Search
  // TODO: Commit
  // TODO: Browser / Editor
  const tableRef = useRef({} as ElemContextTable);
  const cfgSock = useWebSocket(props.socketUrl, {onmessage: onMessage, onclose: () => (tableRef.current = {})});

  async function onMessage(ev: MessageEvent) {
    // TODO: Parse message

    // TODO:
  }

  if (cfgSock?.readyState != WebSocket.OPEN)
    return (<div className='text-center p-3 text-primary'><Spinner animation='border'></Spinner></div>);

  return (
    <div>
      CONNECTED
    </div>
  )
};
