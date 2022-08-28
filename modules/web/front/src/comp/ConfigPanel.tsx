import {createContext, useEffect, useRef, useState} from "react";
import {useForceUpdate, useWebSocket} from "../Utils";
import {Spinner} from "react-bootstrap";

interface ElemDesc {
  elemID: string;
  description: string;
  name: string;
  path: string[]; // Prefix, which is full-key.
  rootName: string;
  initValue: any;
  minValue: any | null;
  maxValue: any | null;
  oneOf: any[] | null;
  editMode: string;
}

interface ElemContextTable {
  [key: number]: { desc: ElemDesc, value: any, updateFence: number }
}

interface ElemUpdateList {
  [key: number]: { value: any, updateFence: number };
}

interface RootTable {
  [key: string]: {
    all: ElemContextTable,
    children: (ElemDesc | string)[];
  }
}

function ElemNode(props: {
  configID: string;
  contextTable: ElemContextTable;
}) {
}

function EditorPopup(props: {
  configID: string;
  targetContext: ElemDesc;
}) {

}

interface ReceivedMessage {
  method: "update" | "new-root" | "delete-root" | "new-cfg";
  params: ElemUpdateList | string[] | string | ElemDesc[];
}

export default function ConfigPanel(props: { socketUrl: string }) {
  // TODO: Search
  // TODO: Commit
  // TODO: Browser / Editor
  const tableRef = useRef({} as ElemContextTable);
  const forceUpdate = useForceUpdate();
  const cfgSock = useWebSocket(props.socketUrl, {
    onopen: ev => forceUpdate(),
    onmessage: onMessage,
    onclose: () => {
      tableRef.current = {};
      forceUpdate();
    }
  });

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
