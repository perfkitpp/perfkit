import {CSSProperties, ReactNode, useContext, useEffect, useMemo, useReducer, useRef, useState} from "react";
import {CategoryDesc, ConfigPanelControlContext, ElemContext, ElemDesc, RootContext} from "./ConfigPanel";
import {Col, InputGroup, Row, Spinner} from "react-bootstrap";
import 'vanilla-jsoneditor/themes/jse-theme-dark.css'
import {JSONEditor} from "vanilla-jsoneditor";
import {EmptyFunc, useForceUpdate, useTimeout} from "../Utils";
import {theme} from "../App";
import findValueStyle from "./Style";

export interface CategoryVisualProps {
  collapsed: boolean
}

export const DefaultVisProp: CategoryVisualProps = {
  collapsed: true
}

const cssIconStyle: CSSProperties = {
  fontSize: '1.1em'
};

function ValueLabel(prop: { rootName: string, elem: ElemContext, prefix?: string }) {
  const valueLabelBg = {
    none: '#00000000',
    dirty: theme.warning + '22',
    comitted: theme.info + '33',
    updated: theme.success + '33',
  }

  const {elem, prefix} = prop;
  const labelRef = useRef(null as any as HTMLDivElement);
  const refreshValueDisplay = useRef(EmptyFunc);
  const refreshFullEditWindow = useRef(EmptyFunc);
  const setHoveringState = useRef(EmptyFunc as (newState: boolean) => void);
  const [inlineEditMode, setInlineEditMode] = useState(false);
  const [fullEditMode, setFullEditMode] = useState(false);
  const forceUpdate = useForceUpdate();

  function stateColor() {
    if (elem.committed) {
      return valueLabelBg.comitted;
    } else if (elem.editted) {
      return valueLabelBg.dirty;
    } else {
      return valueLabelBg.none;
    }
  }

  function setBgColor(color: string, timing_ms: number) {
    if (!labelRef.current)
      return;

    labelRef.current.style.transition = 'background ' + timing_ms + 'ms';
    labelRef.current.style.background = color;
  }

  useEffect(() => {
    setBgColor(stateColor(), 300);

    elem.onCommit = () => {
      setBgColor(valueLabelBg.comitted, 200);
      forceUpdate();
    };
    elem.onChangeDiscarded = () => {
      setBgColor(stateColor(), 1000);
      forceUpdate();
    }

    let timerId: any;
    elem.onUpdateReceived = () => {
      setBgColor(theme.success + '22', 0);
      refreshValueDisplay.current();
      refreshFullEditWindow.current();

      try {
        clearTimeout(timerId);
      } catch {
      }

      timerId = setTimeout(() => {
        setBgColor(stateColor(), 300);
      }, 50);
    };

    elem.onUpdateDiscarded = () => {
      elem.committed = undefined;
      setBgColor(theme.danger + "88", 0);
      refreshValueDisplay.current();
      refreshFullEditWindow.current();

      setTimeout(() => {
        setBgColor(stateColor(), 500);
      }, 200);
    }
    return () => {
      elem.onCommit = EmptyFunc;
      elem.onChangeDiscarded = EmptyFunc;
      elem.onUpdateReceived = EmptyFunc;
      elem.onUpdateDiscarded = EmptyFunc;
    };
  }, []);

  function determineValueClass() {
    return findValueStyle(elem.value);
  }

  function markDirty() {
    elem.editted = true;
    panelContext.markAnyItemDirty(prop.rootName, elem.props.elemID);
    labelRef.current.style.transition = 'background 1s'
    labelRef.current.style.background = stateColor();
  }

  function performRollback(ev: { stopPropagation: () => void }) {
    ev.stopPropagation();

    elem.editted = undefined;
    elem.valueLocal = structuredClone(elem.value);
    panelContext.rollbackDirtyItem(prop.rootName, elem.props.elemID);
    labelRef.current.style.background = stateColor();
    forceUpdate();
  }

  const [iconClass, titleColor] = determineValueClass();
  const panelContext = useContext(ConfigPanelControlContext);

  function onClick() {
    if (fullEditMode) {
      setFullEditMode(false);
      return;
    }

    if (elem.props.oneOf != null) {
      setInlineEditMode(false);
      setFullEditMode(true);
      return;
    }

    switch (typeof elem.value) {
      case "boolean":
        elem.valueLocal = !elem.valueLocal;
        markDirty();
        forceUpdate();
        break;

      case "number":
      case "bigint":
      case "string":
        setInlineEditMode(true);
        break;

      case "object":
        setInlineEditMode(false);
        setFullEditMode(true);
        break;

      default:
        setInlineEditMode(false);
        break;
    }
  }

  function InlineEditMode() {
    const inputRef = useRef(null as null | HTMLInputElement);
    const hasInput = useRef(false);

    useEffect(() => {
      if (inputRef.current == null)
        return;

      if (!elem.editted)
        elem.valueLocal = structuredClone(elem.value);

      const current = inputRef.current;
      current.addEventListener('focusout', (evRaw) => {
        const tg = evRaw.currentTarget as HTMLInputElement;
        applyValue(tg);
      });
      inputRef.current?.focus({preventScroll: false});
    }, []);

    function applyValue(input: HTMLInputElement) {
      setInlineEditMode(false);

      if (hasInput.current) {
        elem.valueLocal = typeof elem.value === "number" ? JSON.parse(input.value) : input.value;
        markDirty();
      }
    }

    switch (typeof elem.value) {
      case "number":
      case "bigint":
      case "string":
        return <input className="form-control form-control-sm text-end bg-transparent flex-grow-1 py-0"
                      ref={inputRef}
                      type={typeof elem.value === "number" ? "number" : "text"}
                      defaultValue={elem.valueLocal}
                      onFocus={ev => ev.currentTarget.select()}
                      onInput={ev => hasInput.current = true}
                      onKeyDown={ev => ev.key == "Enter" && applyValue(ev.currentTarget)}/>;

      default:
        // Value inline editor:
        // https://github.com/josdejong/jsoneditor
        setInlineEditMode(false);
        return <span/>;
    }
  }

  function valueStringify(value: any) {
    return typeof value === "string" ? value : JSON.stringify(value);
  }

  function ValueDisplay(props: { refreshValueDisplay: (elem: () => void) => void }) {
    const forceUpdateValue = useForceUpdate();

    useEffect(() => {
      props.refreshValueDisplay(forceUpdateValue);
    }, []);

    const rawText = valueStringify(elem.value);
    return typeof elem.value === "boolean"
      ? (<span className='btn px-3 py-1' tabIndex={0}
               style={{color: titleColor}}
               onKeyDown={ev => (ev.key == " " || ev.key == "Enter") && onClick()}>
        {(elem.editted ? elem.valueLocal : elem.value)
          ? <i className='ri-eye-fill'/>
          : <i className='ri-eye-close-fill'/>}
        </span>)
      : <span className='text-end overflow-hidden btn flex-grow-0 text-nowrap py-1'
              tabIndex={0}
              onFocus={onClick}
              title={JSON.stringify(elem.value, null, ' ')}
              style={{color: titleColor, textOverflow: 'ellipsis', maxWidth: '20em'}}>
        {elem.editted && <span className='ms-2 small text-light'>
            ({valueStringify(elem.valueLocal)}) </span>}
        {elem.committed && <span className='ms-2 small text-warning fst-italic'>
            ({valueStringify(elem.valueCommitted)}) </span>}
        {rawText}
        </span>;
  }

  function HighlightHr(prop: { color: string }) {
    const [isHovering, setIsHovering] = useState(false);
    useEffect(() => {
      setHoveringState.current = setIsHovering;
    }, []);

    return <div className='h-100 flex-grow-1'>
      <hr
        className='rounded-3'
        style={{
          width: isHovering ? '100%' : '0%',
          height: isHovering ? '.2em' : '1em',
          background: prop.color,
          transition: '.5s',
          opacity: isHovering ? .4 : .0,
          margin: '0'
        }}/>
    </div>
  }

  function FullEdit() {
    function OneOfEdit() {
      useEffect(() => {
        refreshFullEditWindow.current = forceUpdate;
        return () => {
          refreshFullEditWindow.current = EmptyFunc;
        };
      }, []);

      if (elem.props.oneOf == null)
        return <div/>;

      const curSrcLabel = valueStringify(elem.value)
      const curValueLabel = valueStringify(elem.valueLocal);
      let index = 0;
      const allNodes = elem.props.oneOf.map(
        entity => {
          const entityText = valueStringify(entity);
          return <Col sm className='p-1' key={++index}>
            <div
              className={'btn w-100 ' + (curSrcLabel == entityText
                ? 'btn-primary'
                : curValueLabel == entityText ? 'btn-warning' : 'btn-outline-secondary')}
              onClick={() => {
                elem.valueLocal = entity;
                markDirty();
                forceUpdate();
              }}>
              {entityText}
            </div>
          </Col>
        }
      );

      return <Row className='p-2 flex-grow-1'>
        {allNodes}
      </Row>
    }

    function ObjectEdit() {
      const editorContainer = useRef(null as any as HTMLDivElement);
      const editor = useRef(null as null | JSONEditor);

      useEffect(() => {
        elem.onUpdateReceivedAfterCommit = (value: any) => {
          editor.current?.set({json: value})
        }

        if (editor.current) {
          return;
        }

        editor.current = new JSONEditor({
          target: editorContainer.current,
          props: {
            content: {
              text: undefined,
              json: elem.editted ? elem.valueLocal : elem.value
            },
            onChange: (updatedContent, previousContent, {contentErrors, patchResult}) => {
              // content is an object { json: JSONData } | { text: string }
              if (patchResult && patchResult.json) {
                elem.editted = true;
                elem.valueLocal = patchResult.json;
                markDirty();
              }
            },
            onBlur: () => {
              refreshValueDisplay.current();
            },
          }
        });

        editor.current?.focus();
        return () => {
          elem.onUpdateReceivedAfterCommit = EmptyFunc;
        }
      }, []);
      return <div ref={editorContainer} className='w-100 jse-theme-dark'/>
    }

    return <div className='ms-1 ps-1 mb-2 d-flex flex-row align-items-start'>
      <i className='ri-edit-line me-1 text-primary'/>
      {elem.props.oneOf != null ? <OneOfEdit/> : <ObjectEdit/>}
    </div>
  }

  return <div className='h6 px-2 py-0 my-1 border-bottom border-dark border-opacity-10'
              title={elem.props.description}
              ref={labelRef}
              onMouseOver={() => setHoveringState.current(true)}
              onMouseLeave={() => setHoveringState.current(false)}>
    <div className={'d-flex align-items-center gap-2 m-0 p-0'}
         style={{color: titleColor}}
         onClick={onClick}>
      <i className={iconClass} style={cssIconStyle}/>
      <span className={'btn text-start d-flex flex-row flex-grow-0 py-1'}>
        {prefix && <span className='text-opacity-75 text-light pe-2 flex-grow-0'>{prefix} /</span>}
        {elem.cachedSearchHighlightText ? elem.cachedSearchHighlightText : prop.elem.props.name}
        {elem.editted &&
            <i className='btn text-danger ri-arrow-go-back-line m-0 ms-2 p-0 px-1'
               style={{fontSize: '0.9em'}}
               onClick={ev => performRollback(ev)}/>}
      </span>
      <HighlightHr color={titleColor}/>
      {inlineEditMode
        ? <InlineEditMode/>
        : <ValueDisplay refreshValueDisplay={fn => refreshValueDisplay.current = fn}/>}
    </div>
    {fullEditMode && <FullEdit/>}
  </div>;
}

let counter = 0;

function CategoryNode(prop: {
  root: RootContext,
  self: CategoryDesc,
  prefix?: string
}) {
  function toggleCollapse() {
    self.visProps.collapsed = !self.visProps.collapsed;
    forceUpdate();
  }

  const {root, self, prefix} = prop;
  const forceUpdate = useForceUpdate();
  const collapsed = self.visProps.collapsed;
  const filterMode = self.cachedIsAnyChildHitSearch !== undefined;
  const selfFilterHit = self.cachedSearchHighlightText !== undefined;
  const isOpen = filterMode ? self.cachedIsAnyChildHitSearch || selfFilterHit : !collapsed;

  const folderIconClass = prop.prefix
    ? isOpen ? 'ri-folders-line' : 'ri-folders-fill'
    : isOpen ? 'ri-folder-open-line' : 'ri-folder-fill';

  useEffect(() => {
    self.onUpdateSearchState = forceUpdate;
  }, []);

  return <div>
    <h6 className='d-flex flex-row align-items-center text-white py-0'
        onClick={toggleCollapse}>
      <i className={folderIconClass + ' px-2 '} style={cssIconStyle}/>
      {prefix && <span className='text-opacity-75 text-light pe-2 flex-grow-0'>{prefix} /</span>}
      <span className='btn py-1 flex-grow-1 text-start text-light fw-bold my-0'>
        {self.cachedSearchHighlightText ? self.cachedSearchHighlightText : self.name}
      </span>
    </h6>
    <div className={'ms-3 mt-1 ps-0'} style={{borderLeft: ''}}>
      {isOpen && self.children.map(
        child => typeof child === "number"
          ? (!filterMode || (root.all[child].cachedSearchHighlightText !== undefined || selfFilterHit)
            ? <ValueLabel key={child} elem={root.all[child]} rootName={root.root.name}/>
            : <span key={child}/>)
          : (child.cachedIsAnyChildHitSearch === false && !selfFilterHit
            ? <span key={child.name}/>
            : <ForwardedCategoryNode key={child.name} root={prop.root} self={child}/>)
      )}
    </div>
  </div>;
}

function ForwardedCategoryNode(prop: {
  root: RootContext,
  self: CategoryDesc,
  prefix?: string
}) {
  const {root, self, prefix} = prop;

  if (self.children.length === 1) {
    const child = self.children[0];
    const selfFilterHit = self.cachedSearchHighlightText !== undefined;

    return typeof child === "number"
      ? (self.cachedIsAnyChildHitSearch === false && !selfFilterHit
        ? <span/>
        : <ValueLabel
          elem={root.all[child]}
          prefix={(prefix ? prefix + ' / ' : '') + self.name}
          rootName={root.root.name}/>)
      : <ForwardedCategoryNode
        root={root}
        self={child}
        prefix={(prefix ? prefix + ' / ' : '') + self.name}/>;
  } else {
    return <CategoryNode root={root} self={self}/>
  }
}

export function RootNode(prop: {
  name: string,
  ctx: RootContext
}) {
  return <div
    className='m-1 p-0 bg-secondary bg-opacity-10 rounded-3'
    style={{
      fontSize: '1.2rem',
      overflowX: 'hidden',
      fontFamily: 'Lucida Console, monospace',
    }}>
    <CategoryNode root={prop.ctx} self={prop.ctx.root}/>
  </div>;
}
