import {createContext, ReactNode, useEffect, useRef, useState} from "react";
import {useForceUpdate, useWebSocket} from "../Utils";
import {Spinner} from "react-bootstrap";
import {AppendTextToTerminal} from "./Terminal";
import * as View from "./ConfigPanel.UI";
import {CategoryVisualProps, DefaultVisProp} from "./ConfigPanel.UI";

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
  triggerUpdate: () => void;
}

export interface CategoryDesc {
  name: string;
  children: (number | CategoryDesc)[];
  visProps: CategoryVisualProps
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

    // Insert to 'all' array
    const ctx = root.all[elem.elemID] = {
      value: elem.initValue,
      elemDesc: elem,
      triggerUpdate: {} as () => void,
      updateFence: 0
    };

    //
    const setTriggerUpdate = (fn: () => void) => (ctx.triggerUpdate = fn);
    let currentCategory = root.root;

    // iterate path elements, find suitable category.
    for (const str of elem.path) {
      let found = false;

      for (const child of currentCategory.children) {
        if (typeof child === "number")
          continue;

        if (str === child.name) {
          found = true;
          currentCategory = child;
          break;
        }
      }

      if (!found) {
        const children = currentCategory.children;
        currentCategory = {
          name: str,
          children: [],
          visProps: DefaultVisProp
        };
        children.push(currentCategory);
      }
    }

    // Now this handle points terminal node, thus push id here.
    currentCategory.children.push(elem.elemID);
  }
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
            root: {name: value, children: [], visProps: DefaultVisProp}
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

  const allRootFrames = Object.keys(rootTablesRef.current).sort().map(
    key => <View.RootNode key={key} name={key} ctx={rootTablesRef.current[key]}/>
  );

  // TODO: Implement search using 'https://github.com/farzher/fuzzysort'
  return (
    <div style={{
      maxHeight: '70vh',
      overflowY: 'auto',
      overflowX: 'hidden'
    }}>
      <hr/>
      {allRootFrames}
    </div>
  )
};
