import {ReactNode} from "react";
import {ElemDesc} from "./ConfigPanel";
import {Spinner} from "react-bootstrap";

export function RootFrame(prop: {
  children?: ReactNode
}) {
  // TODO: Collapse support
  return (
    <div>
      {prop.children}
    </div>
  );
}

export function CreateNodeLabel(elem: ElemDesc) {
  return <Spinner animation={'grow'}></Spinner>;
}
