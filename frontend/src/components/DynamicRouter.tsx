import { Route } from "@solidjs/router";
import { Component, JSXElement, createContext, createSignal, Accessor, For, useContext } from "solid-js";

export interface MxDynamicRoute {
	path: string;
	component: Component;
};

export interface MxDynamicRouterContext {
	routes: Accessor<Array<MxDynamicRoute>>;
	addRoute: (path: string, component: Component) => void;
};

export const DynamicRouterContext = createContext<MxDynamicRouterContext>();
export const DynamicRouterProvider : Component<{ children: JSXElement }> = (props) => {
	const [routes, setRoutes] = createSignal<Array<MxDynamicRoute>>(new Array<MxDynamicRoute>());

	const addRoute = (path: string, component: Component) => {
		setRoutes((p) => [...p, { path: '/dynamic' + path, component: component }]);
	};

	return <DynamicRouterContext.Provider value={{ routes: routes, addRoute: addRoute }}>{props.children}</DynamicRouterContext.Provider>
};

export const DynamicRouter : Component = () => {
	const { routes } = useContext(DynamicRouterContext) as MxDynamicRouterContext;
	return (
		<For each={routes()}>{(route) => {
			return <Route path={route.path} component={route.component}/>
		}}</For>

	);
};
