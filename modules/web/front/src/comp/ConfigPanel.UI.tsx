import {CSSProperties, ReactNode, useContext, useEffect, useMemo, useReducer, useRef, useState} from "react";
import {CategoryDesc, ConfigPanelControlContext, ElemContext, ElemDesc, RootContext} from "./ConfigPanel";
import {InputGroup, Spinner} from "react-bootstrap";
import {Style} from "util";
import {EmptyFunc, useForceUpdate, useTimeout} from "../Utils";

export interface CategoryVisualProps {
  collapsed: boolean
}

export const DefaultVisProp: CategoryVisualProps = {
  collapsed: true
}

const cssIconStyle: CSSProperties = {
  fontSize: '1.1em'
};

const curStyle = getComputedStyle(document.body);
const theme = {
  primary: curStyle.getPropertyValue('--bs-primary'),
  secondary: curStyle.getPropertyValue('--bs-secondary'),
  success: curStyle.getPropertyValue('--bs-success'),
  info: curStyle.getPropertyValue('--bs-info'),
  warning: curStyle.getPropertyValue('--bs-warning'),
  danger: curStyle.getPropertyValue('--bs-danger'),
  light: curStyle.getPropertyValue('--bs-light'),
  dark: curStyle.getPropertyValue('--bs-dark'),
}

const valueLabelBg = {
  none: '#00000000',
  dirty: theme.warning + '22',
  comitted: theme.warning + '75',
  updated: theme.success + '33',
}

function ValueLabel(prop: { rootName: string, elem: ElemContext, prefix?: string }) {
  const {elem, prefix} = prop;
  const labelRef = useRef(null as any as HTMLDivElement);
  const refrshChild = useRef(EmptyFunc);
  const [inlineEditMode, setInlineEditMode] = useState(false);
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
      refrshChild.current();

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
      refrshChild.current();

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

  function determineValueClass(): [string, `rgb(${number}, ${number}, ${number})` | `#${string}`] {
    switch (typeof elem.value) {
      case "object":
        if (elem.value instanceof Array)
          return ["ri-brackets-line", '#8d740e'];
        else
          return ["ri-braces-line", '#0bab7e'];
      case "boolean":
        return ["ri-checkbox-line", '#0184cb'];
      case "number":
      case "bigint":
        return ["ri-hashtag", '#109675'];
      case "string":
        return ["ri-text", '#ea6a21'];
      default:
        return ["", '#123212'];
    }
  }

  function markDirty() {
    elem.editted = true;
    panelContext.markAnyItemDirty(prop.rootName, elem.props.elemID);
    labelRef.current.style.transition = 'background 1s'
    labelRef.current.style.background = stateColor();
  }

  function performRollback() {
    elem.editted = undefined;
    elem.valueLocal = structuredClone(elem.value);
    panelContext.rollbackDirtyItem(prop.rootName, elem.props.elemID);
    labelRef.current.style.background = stateColor();
    forceUpdate();
  }

  const [iconClass, titleColor] = determineValueClass();
  const panelContext = useContext(ConfigPanelControlContext);

  function onClick() {
    switch (typeof elem.value) {
      case "number":
      case "string":
        setInlineEditMode(true);
        break;
      case "boolean":
        elem.valueLocal = !elem.valueLocal;
        markDirty();
        forceUpdate();
        break;
      default:
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

    if (elem.props.oneOf != null) {
      // TODO: Create dropdown ...
      return <div>ONEOF!</div>;
    }

    switch (typeof elem.value) {
      case "number":
      case "string":
        return <input className="form-control form-control-sm text-end bg-transparent flex-grow-1"
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
      ? (<span className='btn px-3 text-primary' onClick={onClick} tabIndex={0}
               onKeyDown={ev => (ev.key == " " || ev.key == "Enter") && onClick()}>
        {(elem.editted ? elem.valueLocal : elem.value)
          ? <i className='ri-eye-fill'/>
          : <i className='ri-eye-close-fill'/>}
        </span>)
      : <span className='text-end overflow-hidden btn flex-grow-0 text-nowrap'
              tabIndex={0}
              onFocus={onClick}
              title={rawText}
              style={{color: titleColor, textOverflow: 'ellipsis'}}
              onClick={onClick}>
        {rawText}
        {elem.editted && <span className='ms-2 small text-secondary'>
            ({valueStringify(elem.valueLocal)})
        </span>}
        {elem.committed && <span className='ms-2 small text-dark fst-italic'>
            ({valueStringify(elem.valueCommitted)})
        </span>}
        </span>;
  }

  function HighlightHr(prop: { color: string }) {
    const [isHovering, setIsHovering] = useState(false);

    return <div className='h-100 flex-grow-1'
                onMouseEnter={() => setIsHovering(true)}
                onMouseLeave={() => setIsHovering(false)}>
      <hr style={{flexGrow: 2, height: isHovering ? 4 : 1, background: titleColor}}/>
    </div>
  }

  return <div className='h6 px-2 py-0 my-1 rounded-3'
              title={elem.props.description}
              ref={labelRef}>
    <div className={'d-flex align-items-center gap-2 m-0 p-0'} style={{color: titleColor}}>
      <i className={iconClass} style={cssIconStyle}/>
      <span className={'btn text-start d-flex flex-row'}
            style={{flexGrow: 0}}
            onClick={onClick}>
        {prefix && <span className='text-opacity-75 text-secondary'>{prefix} / </span>}
        {prop.elem.props.name}
        {elem.editted &&
            <i className='btn text-danger ri-arrow-go-back-line m-0 ms-2 p-0 px-1'
               style={{fontSize: '0.9em'}}
               onClick={performRollback}/>}
      </span>
      <HighlightHr color={titleColor}/>
      {inlineEditMode ? <InlineEditMode/> : <ValueDisplay refreshValueDisplay={fn => refrshChild.current = fn}/>
      }
    </div>
  </div>;
}

let counter = 0;

function CategoryNode(prop: {
  root: RootContext,
  self: CategoryDesc,
  prefix?: string
}) {
  const {root, self, prefix} = prop;
  const forceUpdate = useForceUpdate();
  const collapsed = self.visProps.collapsed;

  const folderIconClass = prop.prefix
    ? !collapsed ? 'ri-folders-line' : 'ri-folders-fill'
    : !collapsed ? 'ri-folder-open-line' : 'ri-folder-fill';

  function toggleCollapse() {
    self.visProps.collapsed = !self.visProps.collapsed;
    forceUpdate();
  }

  // TODO: Tree Collapse button
  return <div>
    <h6 className='d-flex flex-row align-items-center text-dark fw-bold py-0'
        onClick={toggleCollapse}>
      <i className={folderIconClass + ' px-2 '} style={cssIconStyle}/>
      {prefix && <span>sss?</span>}
      <span className='btn flex-grow-1 text-start my-0' style={{color: "gray"}}>{self.name}</span>
    </h6>
    <div className={'ms-2 mt-1 ps-3'} style={{borderLeft: 'solid 1px lightgray'}}>
      {!collapsed && self.children.map(
        child => typeof child === "number"
          ? <ValueLabel key={child} elem={root.all[child]} rootName={root.root.name}/>
          : <ForwardedCategoryNode key={child.name} root={prop.root} self={child}/>
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
    return typeof child === "number"
      ? <ValueLabel
        elem={root.all[child]}
        prefix={(prefix ? prefix + ' / ' : '') + self.name}
        rootName={root.root.name}/>
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
    className='my-2 px-2'
    style={{
      fontSize: '1.2em',
      overflowX: 'hidden',
      fontFamily: 'Lucida Console, monospace',
    }}>
    <ForwardedCategoryNode root={prop.ctx} self={prop.ctx.root}/>
    <hr/>
  </div>;
}
