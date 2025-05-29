import { Component } from "solid-js";
import { DynamicTitle } from "./components/DynamicTitle";
import Sidebar from "./components/Sidebar";
import { gLoggedUser } from "./lib/globalstate";
import Card from "./components/Card";
import { MxButton } from "./api";
import { MxWebsocket } from "./lib/websocket";
import { MxGenericType } from "./lib/convert";

export const Dashboard : Component = () => {
	return (
		<div>
			<DynamicTitle title="Dashboard"/>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<div class="columns-1 xl:columns-2 gap-5">
					<Card title="Info">
						{gLoggedUser()}
					</Card>
					<Card title="Info">
						<MxButton onClick={() => {
							MxWebsocket.instance.rpc_call('mulex::PdbUserCreate', [
								MxGenericType.str512('test_user'),
								MxGenericType.str512('test_password'),
								MxGenericType.str512('user')
							]);
						}}>
							Create User
						</MxButton>
						<MxButton onClick={() => {
							MxWebsocket.instance.rpc_call('mulex::PdbUserDelete', [
								MxGenericType.str512('test_user')
							]);
						}}>
							Delete User
						</MxButton>
						<MxButton onClick={() => {
							MxWebsocket.instance.rpc_call('mulex::PdbUserChangePassword', [
								MxGenericType.str512('test_password'),
								MxGenericType.str512('test_password2')
							]);
						}}>
							Change Password as test_user
						</MxButton>
					</Card>
				</div>
			</div>
		</div>
	);
};
