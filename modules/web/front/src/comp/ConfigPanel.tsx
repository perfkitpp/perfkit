import {createContext, ReactNode, useEffect, useMemo, useRef, useState} from "react";
import {EmptyFunc, useForceUpdate, useWebSocket} from "../Utils";
import {Col, Container, Row, Spinner} from "react-bootstrap";
import {AppendTextToTerminal} from "./Terminal";
import * as View from "./ConfigPanel.UI";
import {CategoryVisualProps, DefaultVisProp} from "./ConfigPanel.UI";
import * as Fuzzysort from "fuzzysort";

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

interface FuzzySortPreparedContext {
  target: string,
  obj: object
}

export interface ElemContext {
  props: ElemDesc;
  value: any;
  valueLocal: any; // A value which is ready to commit.
  valueCommitted?: any; // A value which is ready to commit.
  editted?: true;
  committed?: true;
  updateFence: number;

  onCommit?: () => void;
  onChangeDiscarded?: () => void;
  onUpdateReceivedAfterCommit?: (val: any) => void;
  onUpdateReceived?: () => void;
  onUpdateDiscarded?: () => void;

  cachedSearchHighlightText?: ReactNode;
}

export interface CategoryDesc {
  name: string;
  children: (number | CategoryDesc)[];
  visProps: CategoryVisualProps

  cachedSearchHighlightText?: ReactNode;
  cachedIsAnyChildHitSearch?: boolean;

  onUpdateSearchState?: () => void;
}

export interface ElemUpdateList {
  rootName: string;

  [key: number]: { value: any, updateFence: number };
}

export interface ElemTable {
  [key: number]: ElemContext
}

export interface RootContext {
  all: ElemTable;
  root: CategoryDesc;

  elemFuzzyContexts: FuzzySortPreparedContext[];
  categoryFuzzyContexts: FuzzySortPreparedContext[];
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

interface MethodUpdateDiscarded {
  method: "discarded"
  params: [string, number][]
}


interface MethodDeleteRoot {
  method: "delete-root"
  params: string
}

function registerConfigEntities(root: RootContext, elems: ElemDesc[], commitFn: () => void) {
  for (const i in elems) {
    const elem = elems[i];

    // Insert to 'all' array
    const ctx = root.all[elem.elemID] = {
      value: elem.initValue,
      valueLocal: structuredClone(elem.initValue),
      props: elem,
      updateFence: 0
    };

    {
      const sortCtx = Fuzzysort.prepare(elem.name) as FuzzySortPreparedContext;
      sortCtx.obj = ctx;
      root.elemFuzzyContexts.push(sortCtx);
    }

    //
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

        const sortCtx = Fuzzysort.prepare(str) as FuzzySortPreparedContext;
        sortCtx.obj = currentCategory;
        root.categoryFuzzyContexts.push(sortCtx);
      }
    }

    // Now this handle points terminal node, thus push id here.
    currentCategory.children.push(elem.elemID);
  }
}

// Propagation Context
export interface ConfigPanelControlInfo {
  markAnyItemDirty: (rootName: string, elemId: number) => {}
  rollbackDirtyItem: (rootName: string, elemId: number) => {}
}

export const ConfigPanelControlContext = createContext({} as ConfigPanelControlInfo);
export const ConfigPanelSearchContext = createContext("");
let globalIdGen = 0;

// Overall control flow ...
// 1. 'new-root': Create new root from corresponding entity of RootContext
// 2. 'new-cfg': Add instance to RootContext.all
//   a. Iterate 'path' of it, find its category.
export default function ConfigPanel(props: { socketUrl: string }) {
  const rootTablesRef = useRef({} as { [key: string]: RootContext });
  const changedItems = useRef({} as { [key: number]: string });
  const updateImmediate = useRef(false);
  const [searchText, setSearchText] = useState("");
  const [rootDirtyFlag, setForceRootDirtyFlag] = useState(0);
  const [isAnyItemDirty, setIsAnyItemDirty] = useState(false);
  const forceUpdate = () => setForceRootDirtyFlag(v => v + 1);
  const cfgSock = useWebSocket(props.socketUrl, {
    onopen: ev => forceUpdate(),
    onmessage: onMessage,
    onclose: () => {
      rootTablesRef.current = {};
      forceUpdate();
    }
  }, []);
  const panelManipContext = useMemo(() => ({
    markAnyItemDirty: (root, elemId) => {
      changedItems.current[elemId] = root;
      isAnyItemDirty || setIsAnyItemDirty(true);

      if (updateImmediate.current) {
        commitAllChanges();
      }
    },
    rollbackDirtyItem: (root, elemId) => {
      delete changedItems.current[elemId];
      const anyItemDirty = (Object.keys(changedItems.current).length > 0);
      if (isAnyItemDirty != anyItemDirty)
        setIsAnyItemDirty(anyItemDirty)
    }
  } as ConfigPanelControlInfo), [cfgSock]);

  async function onMessage(ev: MessageEvent) {
    const roots = rootTablesRef.current;
    let msg = JSON.parse(ev.data) as
      MethodNewCfg | MethodNewRoot | MethodUpdate | MethodDeleteRoot | MethodUpdateDiscarded;
    switch (msg.method) {
      case "new-root":
        AppendTextToTerminal(`configs: New root terminal '${msg.params}'`, '')
        msg.params.forEach(value => {
          const newRoot = roots[value] = {
            all: {},
            root: {name: value, children: [], visProps: structuredClone(DefaultVisProp)},
            elemFuzzyContexts: [],
            categoryFuzzyContexts: [],
          };

          const initialFuzzy = Fuzzysort.prepare(value) as FuzzySortPreparedContext;
          initialFuzzy.obj = newRoot.root;
          (newRoot.categoryFuzzyContexts as FuzzySortPreparedContext[]).push(initialFuzzy);
        })
        forceUpdate();
        break;

      case "new-cfg":
        AppendTextToTerminal(`configs: Registering ${msg.params.length} config entities`, '')
        const [rootName, ...elemDescs] = msg.params;
        registerConfigEntities(roots[rootName], elemDescs, commitAllChanges);
        forceUpdate();
        break;

      case "update":
        // From root, iterate each element, update content. Touch updated elements
        const root = rootTablesRef.current[msg.params.rootName];
        for (const strKey in msg.params) {
          const key = Number(strKey);
          if (isNaN(key))
            continue;

          const elem = msg.params[key];
          const elemCtx = root.all[key];
          elemCtx.value = elem.value;
          elemCtx.updateFence = elem.updateFence;
          if (elemCtx.committed) {
            elemCtx.editted = undefined
            elemCtx.valueLocal = structuredClone(elemCtx.value)
            elemCtx.onUpdateReceivedAfterCommit && elemCtx.onUpdateReceivedAfterCommit(elemCtx.value);
          }
          elemCtx.committed = undefined;
          elemCtx.onUpdateReceived && elemCtx.onUpdateReceived();
        }
        break;

      case "discarded":
        msg.params.map(([rootName, id]) => {
          const root = rootTablesRef.current[rootName];
          const elem = root.all[id];

          elem.onUpdateDiscarded && elem.onUpdateDiscarded();
        });
        break;


      case "delete-root":
        AppendTextToTerminal(`configs: Expiring root '${msg.params}'`);
        forceUpdate();
        break;
    }
  }

  const allRootFrames = useMemo(() =>
      <Container fluid className='p-0'>
        <Row className='m-0 p-0'>
          {Object.keys(rootTablesRef.current).sort().map(key => rootTablesRef.current[key]).map(
            table =>
              table.root.cachedIsAnyChildHitSearch === false
                ? <span key={table.root.name}/>
                : <Col key={table.root.name} style={{minWidth: '60ch', maxWidth: '120ch'}} className='m-0 p-0'>
                  <View.RootNode name={table.root.name} ctx={table}/>
                </Col>
          )}
        </Row>
      </Container>
    , [cfgSock?.readyState, rootDirtyFlag]);

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

  function iterateChanges(func: (elem: ElemContext, root: RootContext) => void) {
    for (const id in changedItems.current) {
      const rootName = changedItems.current[id];
      const root = rootTablesRef.current[rootName];
      const elem = root.all[id];

      func(elem, root);
    }
  }

  function commitAllChanges() {
    const commit = {
      method: 'commit',
      params: [] as any[]
    };

    iterateChanges((elem, root) => {
      const commitArg = [
        root.root.name,
        elem.props.elemID,
        elem.valueLocal
      ];

      commit.params.push(commitArg);
      elem.committed = true;
      elem.editted = undefined;
      elem.valueCommitted = structuredClone(elem.valueLocal);
      elem.onCommit && elem.onCommit();
    })

    cfgSock?.send(JSON.stringify(commit));
    changedItems.current = {};
    setIsAnyItemDirty(false);
  }

  function discardAllChanges() {
    iterateChanges((elem,) => {
      elem.editted = undefined;
      elem.valueLocal = structuredClone(elem.value);
      elem.onChangeDiscarded && elem.onChangeDiscarded();
    })

    changedItems.current = {};
    setIsAnyItemDirty(false);
  }

  const searchBarRef = useRef({} as HTMLInputElement);

  function onKeyDown(ev: KeyboardEvent) {
    if (ev.ctrlKey && ev.key === "Enter") {
      commitAllChanges()
    } else if (ev.key === "Escape") {
      setSearchText('');
      searchBarRef.current.focus();
    }
  }

  useEffect(function onUpdateSearchText() {
    // TODO:
    // console.log("Search Text Changed");
    const now = Date.now();
    const roots = Object.keys(rootTablesRef.current).map(key => rootTablesRef.current[key]);

    // Iterate every object, clear cache state
    for (const root of roots) {
      Object.values(root.all).forEach((elem: ElemContext) => elem.cachedSearchHighlightText = undefined);
      resetCategoryRecursive(root.root);
    }

    if (searchText.length == 0) {
      roots.forEach(root => notifyRecursive(root.root));
      forceUpdate();
      return;
    }

    for (const root of roots) {
      const elemRes = Fuzzysort.go(searchText, root.elemFuzzyContexts, {});
      const categoryRes = Fuzzysort.go(searchText, root.categoryFuzzyContexts, {});


      elemRes.forEach(res => {
        const highlightText = Fuzzysort.highlight(res, highlighter);
        const element = ((res as any).obj as ElemContext);
        element.cachedSearchHighlightText = highlightText;
      });

      categoryRes.forEach(res => {
        const highlightText = Fuzzysort.highlight(res, highlighter);
        const element = ((res as any).obj as CategoryDesc);
        element.cachedSearchHighlightText = highlightText;
      });

      root.root.cachedIsAnyChildHitSearch = refreshChildSearchStateCache(root, root.root);
      notifyRecursive(root.root);

      // console.log(root.root);
    }

    // console.log("seach done.", Date.now() - now, "ms");

    function highlighter(str: string) {
      return <b className='text-danger' key={++globalIdGen}>{str}</b>;
    }

    function notifyRecursive(desc: CategoryDesc) {
      desc.onUpdateSearchState && desc.onUpdateSearchState();
      for (const child of desc.children)
        if (typeof child != "number")
          notifyRecursive(child);
    }

    function refreshChildSearchStateCache(root: RootContext, desc: number | CategoryDesc) {
      if (typeof desc === "number") {
        return root.all[desc].cachedSearchHighlightText !== undefined;
      }


      let hitAnyChild = desc.cachedSearchHighlightText !== undefined;
      desc.children.forEach(child => {
        hitAnyChild = refreshChildSearchStateCache(root, child) || hitAnyChild;
      });

      desc.cachedIsAnyChildHitSearch = hitAnyChild;
      return hitAnyChild;
    }

    function resetCategoryRecursive(desc: CategoryDesc) {
      desc.cachedIsAnyChildHitSearch = undefined;
      desc.cachedSearchHighlightText = undefined;
      desc.children.filter(v => typeof (v) !== "number").forEach((ev: any) => resetCategoryRecursive(ev));
    }

    forceUpdate();
  }, [searchText]);

  const iconFontSize = '1.4em';

  if (cfgSock?.readyState != WebSocket.OPEN)
    return (<div className='text-center p-3 text-primary'><Spinner animation='border'></Spinner></div>);

  // TODO: Implement search using 'https://github.com/farzher/fuzzysort'
  return (
    <ConfigPanelControlContext.Provider value={panelManipContext}>
      <div style={{display: 'flex', flexDirection: 'column', height: '100%', outline: 'none'}}
           tabIndex={0}
           onKeyDown={ev => onKeyDown(ev.nativeEvent)}>
        <span className='d-flex mt-2 flex-row-reverse align-items-center'>
          <span className='w-auto d-flex flex-row-reverse flex-grow-1'>
            <button
              className={'btn ri-upload-2-line p-1 px-5 m-0 me-1 flex-grow-1 '
                + (isAnyItemDirty ? 'btn-primary' : 'btn-outline-primary')}
              style={{fontSize: iconFontSize}}
              title='Commit Changes (Control + Enter)'
              disabled={!isAnyItemDirty}
              onClick={commitAllChanges}/>
            <i
              className='btn ri-arrow-go-back-fill p-1 px-2 m-0 me-1 text-danger'
              style={{fontSize: iconFontSize}}
              title='Discard Changes' onClick={discardAllChanges} hidden={!isAnyItemDirty}/>
            <i
              className={'btn ri-refresh-line p-1 px-2 m-0 me-1 ' + (!updateImmediate.current ? 'text-primary' : 'btn-primary')}
              style={{fontSize: iconFontSize}}
              title='Apply changes immediately'
              onClick={() => (updateImmediate.current = !updateImmediate.current, forceUpdate())}
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
          <span className='flex-grow-0 ms-2 p-1 w-50 d-flex flex-row align-items-center'>
            <i className='ri-search-line'/>
            <input type='text' className='form-control p-0 me-2 ms-3'
                   value={searchText} style={{border: 0}} ref={searchBarRef}
                   onInput={ev => setSearchText(ev.currentTarget.value)}/>
          </span>
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
