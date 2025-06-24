import { Component, For, Show, createSignal, onMount } from "solid-js";
import { DynamicTitle } from "./components/DynamicTitle";
import Sidebar from "./components/Sidebar";
import { gLoggedUser, gLoggedUserRole } from "./lib/globalstate";
import Card from "./components/Card";
import { MxButton } from "./api";
import { MxWebsocket } from "./lib/websocket";
import { MxGenericType } from "./lib/convert";
import { MxPopup } from "./components/Popup";
import DeleteIcon from './assets/delete.svg';
import DeleteIconBlack from './assets/deleteB.svg';
import KeyIcon from './assets/key.svg';
import AvatarIcon from './assets/avatar.svg';
import LogoutIcon from './assets/logout.svg';
import AddIcon from './assets/add.svg';
import { MxFileDropUpload } from "./components/FileDropUpload";
import { Avatar, AvatarFallback, AvatarImage } from "./components/ui/avatar";
import roles from '../../network/roles.json';

interface IRole {
	name: string;
	allowed: boolean;
};

interface IUser {
	name: string;
	allowed: boolean;
};

export const Dashboard : Component = () => {
	const [accCreatePopup, setAccCreatePopup] = createSignal<boolean>(false);
	const [accDeletePopup, setAccDeletePopup] = createSignal<boolean>(false);
	const [accDelPopup, setAccDelPopup] = createSignal<boolean>(false);
	const [accChangePopup, setAccChangePopup] = createSignal<boolean>(false);
	const [accAvatarPopup, setAccAvatarPopup] = createSignal<boolean>(false);
	const [username, setUsername] = createSignal<string>('');
	const [oldPassword, setOldPassword] = createSignal<string>('');
	const [newPassword, setNewPassword] = createSignal<string>('');
	const [newPasswordConf, setNewPasswordConf] = createSignal<string>('');
	const [changeFailed, setChangeFailed] = createSignal<string>('');
	const [changeOK, setChangeOK] = createSignal<string>('');
	const [avatarUrl, setAvatarUrl] = createSignal<string>('');

	const [newUserUsername, setNewUserUsername] = createSignal<string>('');
	const [newUserPassword, setNewUserPassword] = createSignal<string>('');
	const [newUserPasswordConf, setNewUserPasswordConf] = createSignal<string>('');
	const [newUserRole, setNewUserRole] = createSignal<string>('');
	const [userDelete, setUserDelete] = createSignal<string>('');

	const [allRoles, setAllRoles] = createSignal<Array<IRole>>([]);
	const [allUsers, setAllUsers] = createSignal<Array<IRole>>([]);

	onMount(async () => {
		const path = await MxWebsocket.instance.rpc_call('mulex::PdbUserGetAvatarPath');
		setAvatarUrl(path.astype('string'));
		setupRoles();
		await setupUsers();
	});

	function setupRoles() {
		const idx = gLoggedUserRole().id - 1;
		setAllRoles(roles.roles.map((x, i) => { return { name: x.name, allowed: i >= idx }; }));
	}

	async function setupUsers() {
		const id = gLoggedUserRole().id;

		const data = await MxWebsocket.instance.rpc_call('mulex::PdbUserGetAllUsers', [], 'generic');
		const users = data.unpack(['str512', 'int32']);

		const users_list = new Array<IUser>();
		for(const user of users) {
			users_list.push({ name: user[0], allowed: user[1] >= id });
		}
		setAllUsers(users_list);
	}

	function changePassword(e: Event) {
		e.preventDefault();
		if(newPassword() != newPasswordConf()) {
			setChangeFailed("Passwords don't not match.");
			setTimeout(() => setChangeFailed(''), 5000);
			return;
		}
		
		MxWebsocket.instance.rpc_call('mulex::PdbUserChangePassword', [
			MxGenericType.str512(oldPassword()),
			MxGenericType.str512(newPassword())
		]).then((res: MxGenericType) => {
			if(!res.astype('bool')) {
				setChangeFailed("Old password incorrect.");
				setTimeout(() => setChangeFailed(''), 5000);
			}
			else {
				setChangeOK("Password changed.");
				setTimeout(() => setChangeOK(''), 5000);
				setNewPassword('');
				setNewPasswordConf('');
				setOldPassword('');
			}
		});
	}

	function createNewUser(e: Event) {
		e.preventDefault();
		if(newUserPassword() != newUserPasswordConf()) {
			setChangeFailed("Passwords don't not match.");
			setTimeout(() => setChangeFailed(''), 5000);
			return;
		}

		MxWebsocket.instance.rpc_call('mulex::PdbUserCreate', [
			MxGenericType.str512(newUserUsername()),
			MxGenericType.str512(newUserPassword()),
			MxGenericType.str512(newUserRole())
		]).then((res: MxGenericType) => {
			if(!res.astype('bool')) {
				setChangeFailed("Failed to create new user.");
				setTimeout(() => setChangeFailed(''), 5000);
			}
			else {
				setChangeOK("New user created.");
				setTimeout(() => setChangeOK(''), 5000);
				setNewUserPassword('');
				setNewUserPasswordConf('');
				setNewUserRole('');
			}
		});
	}

	function deleteUser(e: Event) {
		e.preventDefault();

		if(userDelete() == gLoggedUser()) {
			setChangeFailed("To delete self use the account settings panel.");
			setTimeout(() => setChangeFailed(''), 5000);
			return;
		}

		MxWebsocket.instance.rpc_call('mulex::PdbUserDelete', [MxGenericType.str512(userDelete())]).then((res: MxGenericType) => {
			if(!res.astype('bool')) {
				setChangeFailed("Failed to delete user.");
				setTimeout(() => setChangeFailed(''), 5000);
			}
			else {
				setChangeOK("User deleted.");
				setTimeout(() => setChangeOK(''), 5000);
				setupUsers();
				setUserDelete('');
			}
		});
	}

	return (
		<div>
			<DynamicTitle title="Dashboard"/>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<div class="columns-1 xl:columns-2 gap-5">
					<Card title="Info">
						<div class="flex gap-5">
							<div class="flex gap-3 items-center border rounded-md p-2 bg-gray-300">
								<Avatar>
									<AvatarImage src={avatarUrl()}/>
									<AvatarFallback>{gLoggedUser().toUpperCase().slice(0, 2)}</AvatarFallback>
								</Avatar>
								<div class="justify-center text-center font-semibold">
									{gLoggedUser()}
								</div>
							</div>
							<MxButton onClick={async () => {
								const logoutResponse = await fetch('/api/logout', {
									method: 'POST'
								});

								if(logoutResponse.ok) {
									// We want a refresh so we can GET the new root
									window.location.reload();
								}
							}} class="size-20 place-items-center">
								{/* @ts-ignore */}
								<LogoutIcon class="size-10"/>
								<div class="text-xs font-semibold">Logout</div>
							</MxButton>
						</div>
					</Card>
					<Card title="Account Settings">
						<div class="flex gap-5">
							<MxButton onClick={() => setAccDelPopup(true)} type="error" class="size-20 place-items-center" disabled={gLoggedUser() === 'admin'}>
								{/* @ts-ignore */}
								<DeleteIcon class="size-10"/>
								<div class="text-xs font-semibold">Delete Account</div>
							</MxButton>
							<MxButton onClick={() => setAccChangePopup(true)} class="size-20 place-items-center">
								{/* @ts-ignore */}
								<KeyIcon class="size-10"/>
								<div class="text-xs font-semibold">Change Password</div>
							</MxButton>
							<MxButton onClick={() => setAccAvatarPopup(true)} class="size-20 place-items-center">
								{/* @ts-ignore */}
								<AvatarIcon class="size-10 fill-black"/>
								<div class="text-xs font-semibold">Change Avatar</div>
							</MxButton>
						</div>
					</Card>
					<Show when={gLoggedUserRole().name === 'sysadmin' || gLoggedUserRole().name === 'sysop'}>
						<Card title="Admin Panel">
							<div class="flex gap-5">
								<MxButton onClick={() => setAccCreatePopup(true)} class="size-20 place-items-center">
									{/* @ts-ignore */}
									<AddIcon class="size-10"/>
									<div class="text-xs font-semibold">Create Account</div>
								</MxButton>
								<MxButton onClick={() => setAccDeletePopup(true)} class="size-20 place-items-center">
									{/* @ts-ignore */}
									<DeleteIconBlack class="size-10"/>
									<div class="text-xs font-semibold">Delete Account</div>
								</MxButton>
							</div>
						</Card>
					</Show>
					{
					// <Card title="Notifications">
					// </Card>
					}

					<MxPopup title="Delete account?" open={accDelPopup()} onOpenChange={setAccDelPopup}>
						<div class="place-items-center">
							<div class="place-content-center justify-center font-semibold text-md">
								You are about to delete your account.
							</div>
							<div class="place-content-center justify-center font-semibold text-md">
								Type your username bellow to confirm.
							</div>
							<div class="place-content-center justify-center font-semibold text-lg mt-2 text-red-500">
								This action is irreversible.
							</div>
							<div class="flex my-5">
								<input
									type="text"
									value={username()}
									onInput={(e) => setUsername(e.currentTarget.value)}
									class="w-full border border-red-300 rounded-lg
										   px-4 mr-2 py-2 focus:outline-none focus:ring-2
										   focus:ring-red-500 text-red-500 placeholder-red-200"
									placeholder={gLoggedUser()}
									required
								/>
								<MxButton 
									type="error"
									onClick={() => MxWebsocket.instance.rpc_call('mulex::PdbUserDelete', [MxGenericType.str512(gLoggedUser())])}
									disabled={username() != gLoggedUser()}
								>
									Delete
								</MxButton>
							</div>
						</div>
					</MxPopup>

					<MxPopup title="Change password" open={accChangePopup()} onOpenChange={setAccChangePopup}>
						<div class="place-items-center">
							<Show when={changeFailed() !== ''}>
								<div class="flex justify-center mb-6 border border-red-900 bg-red-300 text-red-900 rounded-lg w-full py-2">
									{changeFailed()}
								</div>
							</Show>
							<Show when={changeOK() !== ''}>
								<div class="flex justify-center mb-6 border border-green-900 bg-green-300 text-green-900 rounded-lg w-full py-2">
									{changeOK()}
								</div>
							</Show>
							<form onSubmit={changePassword} class="space-y-4">
								<div>
									<label class="block text-gray-700 text-sm font-medium mb-1">Old Password</label>
									<input
										type="password"
										autocomplete="current-password"
										value={oldPassword()}
										onInput={(e) => setOldPassword(e.currentTarget.value)}
										class="w-full border border-gray-300 rounded-lg px-4 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
										required
									/>
								</div>
								<div>
									<label class="block text-gray-700 text-sm font-medium mb-1">New Password</label>
									<input
										type="password"
										autocomplete="new-password"
										value={newPassword()}
										onInput={(e) => setNewPassword(e.currentTarget.value)}
										class="w-full border border-gray-300 rounded-lg px-4 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
										required
									/>
								</div>
								<div>
									<label class="block text-gray-700 text-sm font-medium mb-1">Confirm New Password</label>
									<input
										type="password"
										autocomplete="new-password"
										value={newPasswordConf()}
										onInput={(e) => setNewPasswordConf(e.currentTarget.value)}
										class="w-full border border-gray-300 rounded-lg px-4 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
										required
									/>
								</div>
								<button
									type="submit"
									class="w-full bg-blue-600 text-white py-2 rounded-lg hover:bg-blue-700 transition"
								>
									Change Password
								</button>
							</form>
						</div>
					</MxPopup>

					<MxPopup title="Change avatar" open={accAvatarPopup()} onOpenChange={setAccAvatarPopup}>
						<div class="place-items-center">
							<MxFileDropUpload
								limit={1}
								maxSize={1024 * 1024} // 1MB max
								allowedExtensions={['png', 'jpeg', 'jpg', 'gif']}
								onUploadCompleted={async (handles) => {
									const res = await MxWebsocket.instance.rpc_call('mulex::PdbUserChangeAvatar', [
										MxGenericType.str512(handles[0])
									]);

									if(!res.astype('bool')) {
										console.error('Failed to set user avatar');
									}

									setAccAvatarPopup(false); // Close this popup
								}}
							/>
						</div>
						<div class="place-items-center">
						</div>
					</MxPopup>

					<MxPopup title="Create new user" open={accCreatePopup()} onOpenChange={setAccCreatePopup}>
						<div class="place-items-center">
							<Show when={changeFailed() !== ''}>
								<div class="flex justify-center mb-6 border border-red-900 bg-red-300 text-red-900 rounded-lg w-full py-2">
									{changeFailed()}
								</div>
							</Show>
							<Show when={changeOK() !== ''}>
								<div class="flex justify-center mb-6 border border-green-900 bg-green-300 text-green-900 rounded-lg w-full py-2">
									{changeOK()}
								</div>
							</Show>
							<form onSubmit={createNewUser} class="space-y-4">
								<div>
									<label class="block text-gray-700 text-sm font-medium mb-1">Username</label>
									<input
										type="text"
										autocomplete="username"
										value={newUserUsername()}
										onInput={(e) => setNewUserUsername(e.currentTarget.value)}
										class="w-full border border-gray-300 rounded-lg px-4 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
										pattern="[a-z0-9_]+"
										required
									/>
								</div>
								<div>
									<label class="block text-gray-700 text-sm font-medium mb-1">Role</label>
									<select
										class="w-full border border-gray-300 rounded-lg px-4 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
										name="roles"
										value={newUserRole()}
										onInput={(e) => setNewUserRole(e.currentTarget.value)}
										required
									>
										<For each={allRoles()}>{(role) =>
											<option disabled={!role.allowed}>{role.name}</option>
										}</For>
									</select>
								</div>
								<div>
									<label class="block text-gray-700 text-sm font-medium mb-1">Password</label>
									<input
										type="password"
										autocomplete="new-password"
										value={newUserPassword()}
										onInput={(e) => setNewUserPassword(e.currentTarget.value)}
										class="w-full border border-gray-300 rounded-lg px-4 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
										required
									/>
								</div>
								<div>
									<label class="block text-gray-700 text-sm font-medium mb-1">Confirm Password</label>
									<input
										type="password"
										autocomplete="new-password"
										value={newUserPasswordConf()}
										onInput={(e) => setNewUserPasswordConf(e.currentTarget.value)}
										class="w-full border border-gray-300 rounded-lg px-4 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
										required
									/>
								</div>
								<button
									type="submit"
									class="w-full bg-blue-600 text-white py-2 rounded-lg hover:bg-blue-700 transition"
								>
									Create User	
								</button>
							</form>
						</div>
					</MxPopup>

					<MxPopup title="Delete account" open={accDeletePopup()} onOpenChange={setAccDeletePopup}>
						<div class="place-items-center">
							<Show when={changeFailed() !== ''}>
								<div class="flex justify-center mb-6 border border-red-900 bg-red-300 text-red-900 rounded-lg w-full py-2">
									{changeFailed()}
								</div>
							</Show>
							<Show when={changeOK() !== ''}>
								<div class="flex justify-center mb-6 border border-green-900 bg-green-300 text-green-900 rounded-lg w-full py-2">
									{changeOK()}
								</div>
							</Show>
							<form onSubmit={deleteUser} class="space-y-4 w-full">
								<div>
									<label class="block text-gray-700 text-sm font-medium mb-1">Username</label>
									<select
										class="w-full border border-gray-300 rounded-lg px-4 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
										name="users"
										value={userDelete()}
										onInput={(e) => setUserDelete(e.currentTarget.value)}
										required
									>
										<For each={allUsers()}>{(user) =>
											<option disabled={!user.allowed}>{user.name}</option>
										}</For>
									</select>
								</div>
								<button
									type="submit"
									class="w-full bg-red-600 text-white py-2 rounded-lg hover:bg-red-700 transition"
								>
									Delete User	
								</button>
							</form>
						</div>
					</MxPopup>
				</div>
			</div>
		</div>
	);
};
