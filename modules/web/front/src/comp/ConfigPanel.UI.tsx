import {CSSProperties, ReactNode, useContext, useEffect, useMemo, useReducer, useRef, useState} from "react";
import {CategoryDesc, ConfigPanelControlContext, ElemContext, ElemDesc, RootContext} from "./ConfigPanel";
import {InputGroup, Spinner} from "react-bootstrap";
import {Style} from "util";
import {useForceUpdate} from "../Utils";

export interface CategoryVisualProps {
  collapsed: boolean
}

export const DefaultVisProp: CategoryVisualProps = {
  collapsed: true
}

const cssIconStyle: CSSProperties = {
  fontSize: '1.1em'
};

function FullValueEdit() {

}


function ValueLabel(prop: { rootName: string, elem: ElemContext, prefix?: string }) {
  const {elem, prefix} = prop;
  const [inlineEditMode, setInlineEditMode] = useState(false);
  const forceUpdate = useForceUpdate();

  useEffect(() => {
    elem.triggerUpdate = forceUpdate;
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
    forceUpdate();
  }

  const [iconClass, titleColor] = determineValueClass();
  const rawText = typeof elem.value === "string" ? elem.value : JSON.stringify(elem.valueLocal);
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
        break;
      default:
        break;
    }
  }

  function InlineEdit() {
    const inputRef = useRef(null as null | HTMLInputElement);

    useEffect(() => {
      if (inputRef.current == null)
        return;

      const current = inputRef.current;
      current.addEventListener('focusout', () => {
        console.log("OUT!");
        setInlineEditMode(false);
      });
      console.log("SETUP!!");
      inputRef.current?.focus({preventScroll: false});
    }, []);

    if (elem.props.oneOf != null) {
      // TODO: Create dropdown ...
      return <div>ONEOF!</div>;
    }

    switch (typeof elem.value) {
      case "number":
      case "string":
        return <input className="form-control form-control-sm text-end bg-transparent"
                      ref={inputRef}
                      type={elem.value === "number" ? "number" : "text"}
                      defaultValue={rawText}/>;

      default:
        setInlineEditMode(false);
        return <span/>;
    }
  }

  function HighlightHr(prop: { color: string }) {
    const [isHovering, setIsHovering] = useState(false);

    return <div className='h-100 flex-grow-1'
                onMouseEnter={() => setIsHovering(true)}
                onMouseLeave={() => setIsHovering(false)}>
      <hr style={{flexGrow: 2, height: isHovering ? 4 : 1, background: titleColor}}/>
    </div>
  }

  return <div className={'h6 px-2 py-0 my-1' + (elem.editted ? ' bg-warning bg-opacity-25 rounded-3' : '')}>
    <div className={'d-flex align-items-center gap-2 m-0 p-0'} style={{color: titleColor}}>
      <i className={iconClass} style={cssIconStyle}/>
      <span className={'text-start btn '}
            style={{flexGrow: 0}}>
        {(elem.editted ? '*' : '')}
        {prefix && <span className='text-opacity-75 text-secondary'>{prefix} / </span>}
        {prop.elem.props.name}
      </span>
      <HighlightHr color={titleColor}/>
      {inlineEditMode
        ? <span className='flex-grow-0 w-auto'>
          <InlineEdit/>
        </span>
        : typeof elem.value === "boolean"
          ? <span className='btn px-3 text-primary' onClick={onClick}>
              {elem.valueLocal ? <i className='ri-eye-fill'/> : <i className='ri-eye-close-fill'/>}
            </span>
          : <span className='text-end overflow-hidden btn flex-grow-0 text-nowrap'
                  title={rawText}
                  style={{color: titleColor, textOverflow: 'ellipsis'}}
                  onClick={onClick}>
              {rawText}
            </span>
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
  // TODO: Root Collapse support
  // TODO: Render node header
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
