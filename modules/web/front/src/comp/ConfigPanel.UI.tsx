import {CSSProperties, ReactNode, useEffect, useState} from "react";
import {CategoryDesc, ElemContext, ElemDesc, RootContext} from "./ConfigPanel";
import {Spinner} from "react-bootstrap";
import {Style} from "util";

export interface CategoryVisualProps {
  collapsed: boolean
}

export const DefaultVisProp: CategoryVisualProps = {
  collapsed: true
}

const cssIconStyle: CSSProperties = {
  fontSize: '1.1em'
};

function ValueLabel(prop: { elem: ElemContext, prefix?: string }) {
  const {elem, prefix} = prop;

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
        return ["ri-number-1", '#109675'];
      case "string":
        return ["ri-text", '#ea6a21'];
      default:
        return ["", '#123212'];
    }
  }

  const [iconClass, titleColor] = determineValueClass();

  return <div className={'h6'}>
    <div className={'d-flex align-items-center gap-2 '} style={{color: titleColor}}>
      <i className={iconClass} style={cssIconStyle}/>
      {prefix && <span className='fw-bold'>{prefix} /</span>}
      <span>{prop.elem.elemDesc.name}</span>
    </div>
  </div>
}

function CategoryNode(prop: {
  root: RootContext,
  self: CategoryDesc,
  prefix?: string
}) {
  const {root, self, prefix} = prop;
  const [collapsed, setCollapsed] = useState(true);

  useEffect(() => {
    setCollapsed(self.visProps.collapsed)
  }, [self.visProps.collapsed]);

  const folderIconClass = prop.prefix
    ? !collapsed ? 'ri-folders-line' : 'ri-folders-fill'
    : !collapsed ? 'ri-folder-open-line' : 'ri-folder-fill';

  return <div>
    <h6 className='d-flex flex-row align-items-center text-secondary fw-bold'>
      <i className={folderIconClass + ' pe-2'} style={cssIconStyle}/>
      {prefix && <span>sss?</span>}
      <span>{self.name}</span>
    </h6>
    <div className={'ms-3'}>
      {!collapsed && self.children.map(
        child => typeof child === "number"
          ? <ValueLabel key={child} elem={root.all[child]}/>
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
      ? <ValueLabel elem={root.all[child]} prefix={(prefix ? prefix + ' / ' : '') + self.name}/>
      : <ForwardedCategoryNode root={root} self={child} prefix={(prefix ? prefix + ' / ' : '') + self.name}/>;
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
    className='my-2 px-4'
    style={{
      fontSize: '1.2em',
      fontFamily: 'Lucida Console, monospace',
    }}>
    <ForwardedCategoryNode root={prop.ctx} self={prop.ctx.root}/>
    <hr/>
  </div>;
}
