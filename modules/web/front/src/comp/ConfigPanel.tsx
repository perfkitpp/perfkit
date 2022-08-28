import {createContext, ReactNode, useEffect, useRef, useState} from "react";
import {useForceUpdate, useWebSocket} from "../Utils";
import {Spinner} from "react-bootstrap";
import {AppendTextToTerminal} from "./Terminal";
import * as View from "./ConfigPanel.UI";

export interface ElemDesc {
  elemID: number;
  description: string;
  name: string;
  path: string[]; // Prefix, which is full-key.
  initValue: any;
  minValue: any | null;
  maxValue: any | null;
  oneOf: any[] | null;
  editMode: string;
}

export interface ElemContext {
  elemDesc: ElemDesc;
  value: any;
  updateFence: number;
  cachedRendering: ReactNode;
  triggerUpdate: () => void;
}

export interface CategoryDesc {
  children: (number | CategoryDesc)[];
}

export interface ElemUpdateList {
  rootName: string;

  [key: number]: { value: any, updateFence: number };
}

export interface RootContext {
  all: { [key: number]: ElemContext },
  root: CategoryDesc;
}

function ElemNode(props: {
  configID: string;
  contextTable: RootContext;
}) {
}

function EditorPopup(props: {
  configID: string;
  targetContext: ElemDesc;
}) {

}

interface MethodNewRoot {
  method: "new-root"
  params: string[]
}

interface MethodNewCfg {
  method: "new-cfg"
  params: [string, ...ElemDesc[]]
}

interface MethodUpdate {
  method: "update"
  params: ElemUpdateList
}

interface MethodDeleteRoot {
  method: "delete-root"
  params: string
}

function registerConfigEntities(root: RootContext, elems: ElemDesc[]) {
  for (const i in elems) {
    const elem = elems[i];
    const ctx = root.all[elem.elemID] = {
      value: elem.initValue,
      cachedRendering: View.CreateNodeLabel(elem),
      elemDesc: elem,
      triggerUpdate: {} as () => void,
      updateFence: 0
    };

    const setTriggerUpdate = (fn: () => void) => (ctx.triggerUpdate = fn);
    let currentCategory = root.root;
    // TODO: iterate path, find its suitable category.
  }
}

function buildRootTree(root: RootContext) {
  // TODO: Iterate categories recursively, build root tree.

  return <div></div>;
}


// Overall control flow ...
// 1. 'new-root': Create new root from corresponding entity of RootContext
// 2. 'new-cfg': Add instance to RootContext.all
//   a. Iterate 'path' of it, find its category.
export default function ConfigPanel(props: { socketUrl: string }) {
  const rootTablesRef = useRef({} as { [key: string]: RootContext });
  const forceUpdate = useForceUpdate();
  const cfgSock = useWebSocket(props.socketUrl, {
    onopen: ev => forceUpdate(),
    onmessage: onMessage,
    onclose: () => {
      rootTablesRef.current = {};
      forceUpdate();
    }
  });

  async function onMessage(ev: MessageEvent) {
    const roots = rootTablesRef.current;
    let msg = JSON.parse(ev.data) as MethodNewCfg | MethodNewRoot | MethodUpdate | MethodDeleteRoot;
    switch (msg.method) {
      case "new-root":
        AppendTextToTerminal(`configs: New root terminal '${msg.params}'`, '')
        msg.params.forEach(value => {
          roots[value] = {
            all: {},
            root: {children: []}
          };
        })
        forceUpdate();
        break;

      case "new-cfg":
        AppendTextToTerminal(`configs: Registering ${msg.params.length} config entities`, '')
        const [rootName, ...elemDescs] = msg.params;
        registerConfigEntities(roots[rootName], elemDescs);
        forceUpdate();
        break;

      case "update":
        // TODO: From root, iterate each element, update content.
        break;

      case "delete-root":
        AppendTextToTerminal(`configs: Expiring root '${msg.params}'`);
        forceUpdate();
        break;
    }
  }

  if (cfgSock?.readyState != WebSocket.OPEN)
    return (<div className='text-center p-3 text-primary'><Spinner animation='border'></Spinner></div>);

  console.log("Rendering ROOT FRAMES ...");

  const allRootFrames = Object.keys(rootTablesRef.current).map(
    key => {
      const rootCtx = rootTablesRef.current[key];
      const content = buildRootTree(rootCtx);
      return <View.RootFrame key={key}>
        {content}
      </View.RootFrame>
    });

  return (
    <div>
      {allRootFrames}
    </div>
  )
};
