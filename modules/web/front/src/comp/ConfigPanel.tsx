import {createContext, ReactNode, useEffect, useMemo, useRef, useState} from "react";
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
  props: ElemDesc;
  value: any;
  valueLocal: any; // A value which is ready to commit.
  editted?: true;
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

export interface ElemTable {
  [key: number]: ElemContext
}

export interface RootContext {
  all: ElemTable,
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
      valueLocal: structuredClone(elem.initValue),
      props: elem,
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
          visProps: structuredClone(DefaultVisProp)
        };
        children.push(currentCategory);
      }
    }

    // Now this handle points terminal node, thus push id here.
    currentCategory.children.push(elem.elemID);
  }
}

// Propagation Context
export interface ConfigPanelControlInfo {
  markAnyItemDirty: (rootName: string, elemId: number) => {}
}

export const ConfigPanelControlContext = createContext({} as ConfigPanelControlInfo);

// Overall control flow ...
// 1. 'new-root': Create new root from corresponding entity of RootContext
// 2. 'new-cfg': Add instance to RootContext.all
//   a. Iterate 'path' of it, find its category.
export default function ConfigPanel(props: { socketUrl: string }) {
  const rootTablesRef = useRef({} as { [key: string]: RootContext });
  const changedItems = useRef({} as { [key: number]: string });
  const [updateImmediate, setUpdateImmediate] = useState(false);
  const [rootDirtyFlag, setForceRootDirtyFlag] = useState(0);
  const [isAnyItemDirty, setIsAnyItemDirty] = useState(false);
  const forceUpdate = () => setForceRootDirtyFlag(v => v + 1);
  const panelManipContext = useMemo(() => ({
    markAnyItemDirty: (root, elemId) => {
      if (updateImmediate) {
        // TODO: Apply changes immediately ...
      } else {
        changedItems.current[elemId] = root;
        isAnyItemDirty || setIsAnyItemDirty(true)
      }
    }
  } as ConfigPanelControlInfo), []);
  const cfgSock = useWebSocket(props.socketUrl, {
    onopen: ev => forceUpdate(),
    onmessage: onMessage,
    onclose: () => {
      rootTablesRef.current = {};
      forceUpdate();
    }
  }, []);

  async function onMessage(ev: MessageEvent) {
    const roots = rootTablesRef.current;
    let msg = JSON.parse(ev.data) as MethodNewCfg | MethodNewRoot | MethodUpdate | MethodDeleteRoot;
    switch (msg.method) {
      case "new-root":
        AppendTextToTerminal(`configs: New root terminal '${msg.params}'`, '')
        msg.params.forEach(value => {
          roots[value] = {
            all: {},
            root: {name: value, children: [], visProps: structuredClone(DefaultVisProp)}
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
        // TODO: From root, iterate each element, update content. Touch updated elements
        break;

      case "delete-root":
        AppendTextToTerminal(`configs: Expiring root '${msg.params}'`);
        forceUpdate();
        break;
    }
  }

  const allRootFrames = useMemo(() => Object.keys(rootTablesRef.current).sort().map(
    key => <View.RootNode key={key} name={key} ctx={rootTablesRef.current[key]}/>
  ), [cfgSock?.readyState, rootDirtyFlag]);

  if (cfgSock?.readyState != WebSocket.OPEN)
    return (<div className='text-center p-3 text-primary'><Spinner animation='border'></Spinner></div>);

  function setCollapseAll(isCollapsed: boolean) {
    for (const key in rootTablesRef.current) {
      const root = rootTablesRef.current[key];
      recurseCategory(root.all, root.root, e => {
        e.visProps.collapsed = isCollapsed
      });
    }

    forceUpdate();

    function recurseCategory(all: ElemTable, node: CategoryDesc, fn: (e: CategoryDesc) => void) {
      fn(node);
      for (const elem of node.children) {
        if (typeof elem === "number")
          continue;

        recurseCategory(all, elem, fn);
      }
    }
  }

  function commitAllChanges() {
    // TODO: Flush changed items
  }

  function discardAllChanges() {
    // TODO: Iterate all changes, roll back their locals, force update.
  }

  const iconFontSize = '1.4em';

  // TODO: Implement search using 'https://github.com/farzher/fuzzysort'
  return (
    <ConfigPanelControlContext.Provider value={panelManipContext}>
      <div style={{display: 'flex', flexDirection: 'column', height: '100%'}}>
        <span className='d-flex mt-2 flex-row-reverse align-items-center'>
          <span className='w-25 d-flex flex-row-reverse'>
            <button
              className={'btn ri-mail-send-fill p-1 m-0 me-1 flex-grow-1 '
                + (isAnyItemDirty ? 'btn-primary' : 'btn-outline-primary')}
              style={{fontSize: iconFontSize}}
              title='Commit Changes'
              disabled={!isAnyItemDirty}
              onClick={commitAllChanges}/>
            <i
              className='btn ri-arrow-go-back-fill p-1 px-2 m-0 me-1 text-danger'
              style={{fontSize: iconFontSize}}
              title='Discard Changes' onClick={discardAllChanges} hidden={!isAnyItemDirty}/>
            <i
              className={'btn ri-refresh-line p-1 px-2 m-0 me-1 ' + (!updateImmediate ? 'text-primary' : 'btn-primary')}
              style={{fontSize: iconFontSize}}
              title='Apply changes immediately' onClick={() => setUpdateImmediate(v => !v)}
            />
          </span>
          <i
            className='btn ri-line-height p-1 px-2 m-0 me-1'
            title='Expand All'
            style={{fontSize: iconFontSize}}
            onClick={() => setCollapseAll(false)}/>
          <i
            className='btn ri-align-vertically p-1 px-2 m-0 me-1'
            title='Collapse All'
            style={{fontSize: iconFontSize}}
            onClick={() => setCollapseAll(true)}/>
          <span className='flex-grow-1 ms-2 p-1 '>Search Text Here</span>
        </span>
        <hr className='my-1 mx-1'/>
        <div style={{
          overflowY: 'scroll',
          width: '100%'
        }}>
          {allRootFrames}
        </div>
      </div>
    </ConfigPanelControlContext.Provider>
  )
};
